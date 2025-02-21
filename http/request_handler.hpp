#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>
#include <boost/unordered_map.hpp>
#include <boost/any.hpp>
#include <boost/asio.hpp>
#include "context.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace unordered = boost::unordered; // from <boost/unordered_map.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace bst {
    class request_handler
    {
    private:
        /* route map */
        using FuncHandlerResponseString = 
        std::function<net::awaitable<void>(http::request<http::string_body>&, http::response<http::string_body>&,bst::context&)>;
        using FuncHandlerResponseFile = 
        std::function<net::awaitable<void>(http::request<http::string_body>&, http::response<http::file_body>&,bst::context&)>;
        inline static unordered::unordered_map<std::string, boost::any> routes_;
        template<class Body>
        inline static void prepare_response(http::request<http::string_body>& req, http::response<Body>& res)
        {
            //if req require close,we close
            // Set common response headers
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.keep_alive(req.keep_alive());
            res.prepare_payload();
        }
    public:
        static void register_route(const std::string& path, FuncHandlerResponseString handler) {
            routes_[path] = handler;
        }

        static void register_route(const std::string& path, FuncHandlerResponseFile handler) {
            routes_[path] = handler;
        }

        template<class Body, class Fields = http::basic_fields<std::allocator<char>>>
        static net::awaitable<http::message<false, Body, Fields> > handle_request(
            std::shared_ptr<bst::context> const& ctx,
            http::request<Body>&& req) {
            auto it = routes_.find(req.target());
            if (it != routes_.end()) {
                if (it->second.type() == typeid(FuncHandlerResponseString)) {
                    auto handler = boost::any_cast<FuncHandlerResponseString>(it->second);
                    http::response<http::string_body> res{http::status::ok, req.version()};
                    co_await handler(req, res, *ctx);
                    prepare_response(req, res);
                    co_return std::move(res);
                } else if (it->second.type() == typeid(FuncHandlerResponseFile)) {
                    auto handler = boost::any_cast<FuncHandlerResponseFile>(it->second);
                    http::response<http::file_body> res{http::status::ok, req.version()};
                    co_await handler(req, res, *ctx);
                    prepare_response(req, res);
                    co_return std::move(res);
                }
            }
            //else
            auto const not_found = [&req](beast::string_view target) {
                http::response<http::string_body> res{http::status::not_found, req.version()};
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, "text/html");
                res.body() = "The path '" + std::string(target) + "' was not found.";
                res.keep_alive(req.keep_alive());
                res.prepare_payload();
                return std::move(res);
            };
            co_return std::move(not_found(req.target()));
        }
    };
    
} // namespace bst