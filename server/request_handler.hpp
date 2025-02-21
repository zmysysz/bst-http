#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>
#include <boost/unordered_map.hpp>
#include <boost/any.hpp>
#include <boost/asio/spawn.hpp>
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
        std::function<void(http::request<http::string_body>&, http::response<http::string_body>&,bst::context&)>;
        using FuncHandlerResponseFile = 
        std::function<void(http::request<http::string_body>&, http::response<http::file_body>&,bst::context&)>;
        inline static unordered::unordered_map<std::string, boost::any> routes_;
        template<class Body>
        inline static void PrepareResponse(http::request<http::string_body>& req, http::response<Body>& res)
        {
            res.keep_alive(req.keep_alive());
            res.prepare_payload();
        }
    public:
        request_handler(/* args */){};
        ~request_handler(){};
        // Register a route with a handler void handler(http::request<http::string_body>& req, http::response<http::string_body>& res,bst_context &ctx)
        static void register_route(const std::string &path, std::function<void(http::request<http::string_body>&, 
            http::response<http::string_body>&,bst::context&)> handler)
        {
            routes_[path] = handler;
        }
        // Register a route with a handler void handler(http::request<http::string_body>& req, http::response<http::string_body>& res,bst_context &ctx)
        static void register_route(const std::string &path, std::function<void(http::request<http::string_body>&, 
            http::response<http::file_body>&,bst::context&)> handler)
        {
            routes_[path] = handler;
        }
        // This function produces an HTTP response for the given
        // request. The type of the response object depends on the
        // contents of the request, so the interface requires the
        // donot use this function ( framewoke use it )
        template<class Body, class Allocator,class Send>
        static void handle_request(
            std::shared_ptr<bst::context> const& ctx,
            http::request<Body, http::basic_fields<Allocator>>&& req,
            Send&& send,net::yield_context yield)
        {
          
            auto it = request_handler::routes_.find(req.target().to_string());
            if (it != routes_.end()) 
            {
                if (it->second.type() == typeid(FuncHandlerResponseString)) {
                    auto handler = boost::any_cast<FuncHandlerResponseString>(it->second);
                    http::response<http::string_body> res{http::status::ok, req.version()};
                    handler(req, res, *(ctx->sub_ctx));
                    PrepareResponse(req, res);
                    return send(std::move(res));
                  
                } else if (it->second.type() == typeid(FuncHandlerResponseFile)) {
                    auto handler = boost::any_cast<FuncHandlerResponseFile>(it->second);
                    http::response<http::file_body> res{http::status::ok, req.version()};
                    handler(req, res, *(ctx->sub_ctx));
                    PrepareResponse(req, res);
                    return send(std::move(res));
                }
            } 
            else 
            {
                //not find return 404
                auto const not_found = [&req](beast::string_view target)
                {
                    http::response<http::string_body> res{http::status::not_found, req.version()};
                    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(http::field::content_type, "text/html");
                    res.body() = "The path '" + std::string(target) + "' was not found.";
                    res.keep_alive(req.keep_alive());
                    res.prepare_payload();
                    return res;
                };
                return send(not_found(req.target()));
            }
        }
    };
    
} // namespace bst