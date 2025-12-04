#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>
#include <boost/unordered_map.hpp>
#include <boost/any.hpp>
#include <boost/asio.hpp>
#include "http_base.hpp"
#include "context.hpp"


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace unordered = boost::unordered; // from <boost/unordered_map.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace bst {
    class request_context;
    using FuncHandlerResponseString = 
    std::function<net::awaitable<void>(std::shared_ptr<http::request<http::string_body>>
        ,std::shared_ptr<http::response<http::string_body>>
        ,std::shared_ptr<bst::request_context>)>;
    using FuncHandlerResponseFile = 
    std::function<net::awaitable<void>(std::shared_ptr<http::request<http::string_body>>
        ,std::shared_ptr<http::response<http::file_body>>
        ,std::shared_ptr<bst::request_context>)>;
    //
    class request_handler
    {
    private:
        /* route map */ 
        inline static unordered::unordered_map<std::string, boost::any> routes_;
        inline static std::vector<std::pair<std::string, boost::any>> prefix_routes_;
        friend class request_handler_impl;
    public:
        static void register_route(const std::string& path, FuncHandlerResponseString handler, bool prifix_match = false) {
            if(prifix_match) {
                prefix_routes_.emplace_back(path, handler);
            } else {
                routes_[path] = handler;
            }
        }

        static void register_route(const std::string& path, FuncHandlerResponseFile handler, bool prifix_match = false) {
            if(prifix_match) {
                prefix_routes_.emplace_back(path, handler);
            } else {
                routes_[path] = handler;
            }
        }
    };
    //
    struct request_context : bst::context
    {
        request_context( beast::tcp_stream & s) : 
        stream(s),
        req(nullptr),
        res_string(nullptr),
        res_file(nullptr),
        res_type(0),
        auto_response(true) {
        }
        ~request_context() = default;
        std::shared_ptr<http::request<http::string_body>> get_request() { 
            if(req) {
                return req;
            }
            return nullptr;
        }
        std::shared_ptr<http::response<http::string_body>> get_string_response() {
            if(res_type == 0 && res_string) {
                return res_string;
            }
            return nullptr;
        }
        std::shared_ptr<http::response<http::file_body>> get_file_response() { 
            if(res_type == 1 && res_file) {
                return res_file;
            }
            return nullptr;
        }
        std::string get_path() { 
            return path;
        }
        std::string get_prifix_path() {
            return prefix_path;
        }
        bool get_param(const std::string &key,std::string &value) {
            auto it = params.find(key);
            if(it != params.end()) {
                value = it->second;
                return true;
            }
            return false;
        }
        std::string get_param(const std::string &key) {
            auto it = params.find(key);
            if(it != params.end()) {
                return it->second;
            }
            return "";
        }
        std::string get_peer_ip() {
            if(stream.socket().is_open()) {
                return stream.socket().remote_endpoint().address().to_string();
            }
            return "";
        }
        int get_response_type() { 
            return res_type;
        }
        void set_manual_response() { 
            auto_response = false;
        }
        void set_auto_response() { 
            auto_response = true;
        }
        bool is_auto_response() { 
            return auto_response;
        }
        //
        net::awaitable<bool> manual_response() {
            if(!is_auto_response()) {
                co_return co_await response();
            }
            co_return false;
        }
    private:
        net::awaitable<bool> response() {
            bool be_open = true;
            bool be_succ = false;              
            if(res_type == 0 && res_string) {
                response_sender::prepare_response(req,res_string);
                be_succ = co_await response_sender::write(stream,res_string);
                be_open =  response_sender::keep_open(res_string);
                
            } else if(res_type == 1 && res_file) {
                response_sender::prepare_response(req,res_string);
                be_succ = co_await  response_sender::write(stream,res_file);
                be_open =  response_sender::keep_open(res_file);
            }
            if(!be_succ || !be_open) {
                //if response is not success, we close the session
                response_sender::close(stream);
            }
            co_return be_succ;
        }
    private:
        friend class request_handler_impl;
        friend class request_session;
        beast::tcp_stream & stream;
        std::shared_ptr<http::request<http::string_body>> req;
        std::shared_ptr<http::response<http::string_body>> res_string;
        std::shared_ptr<http::response<http::file_body>> res_file;
        std::string path; // the request path
        std::string prefix_path; //the prfix path for perfix mode
        std::map<std::string, std::string> params;
        int res_type = 0; // 0: string, 1: file
        bool auto_response;
    };
    //
    class request_handler_impl : public std::enable_shared_from_this<request_handler_impl>
    {
    public:
        request_handler_impl() = default;
        ~request_handler_impl() = default;
        net::awaitable<int> handle_request(
            std::shared_ptr<request_context> const& ctx,
            std::shared_ptr<http::request<http::string_body>> req) {
            util::parse_request(req->target(), ctx->path, ctx->params);
            //first full match
            auto it = request_handler::routes_.find(ctx->path);
            if (it !=  request_handler::routes_.end()) {
                if (it->second.type() == typeid(FuncHandlerResponseString)) {
                    auto handler = boost::any_cast<FuncHandlerResponseString>(it->second);
                    ctx->res_string = std::make_shared<http::response<http::string_body>>(http::status::ok, req->version());
                    auto res = ctx->res_string;
                    ctx->res_type = 0;
                    co_await handler(req, res, ctx);
                    co_return (int)res->result_int();
                } else if (it->second.type() == typeid(FuncHandlerResponseFile)) {
                    auto handler = boost::any_cast<FuncHandlerResponseFile>(it->second);
                    ctx->res_file = std::make_shared<http::response<http::file_body>>(http::status::ok, req->version());
                    auto res = ctx->res_file;
                    ctx->res_type = 1;
                    co_await handler(req, res, ctx);
                    co_return (int)res->result_int();
                }
            } else {
                //prefix match
                for(auto & [path, any_handler] : request_handler::prefix_routes_) {
                    if(ctx->path.starts_with(path)) {
                        ctx->prefix_path = path;
                        if (any_handler.type() == typeid(FuncHandlerResponseString)) {
                            auto handler = boost::any_cast<FuncHandlerResponseString>(any_handler);
                            ctx->res_string = std::make_shared<http::response<http::string_body>>(http::status::ok, req->version());
                            auto res = ctx->res_string;
                            ctx->res_type = 0;
                            co_await handler(req, res, ctx);
                            co_return (int)res->result_int();
                        } else if (any_handler.type() == typeid(FuncHandlerResponseFile)) {
                            auto handler = boost::any_cast<FuncHandlerResponseFile>(any_handler);
                            ctx->res_file = std::make_shared<http::response<http::file_body>>(http::status::ok, req->version());
                            auto res = ctx->res_file;
                            ctx->res_type = 1;
                            co_await handler(req, res, ctx);
                            co_return (int)res->result_int();
                        }
                    }
                }
            }
            //else
            auto const not_found = [&](beast::string_view target) {
                //http::response<http::string_body> res{http::status::not_found, req.version()};
                ctx->res_string = std::make_shared<http::response<http::string_body>>(http::status::not_found, req->version());
                auto res = ctx->res_string;
                ctx->res_type = 0;
                res->set(http::field::content_type, "text/html");
                res->body() = "The path '" + std::string(target) + "' was not found.";
                return (int)http::status::not_found;
            };
            co_return not_found(req->target());
        }
    };
    
} // namespace bst