#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <iostream>
#include <boost/asio/spawn.hpp>
#include "request_connection.hpp"
#include <thread>
#include "context.hpp"
#include <shared_mutex>
#include <mutex>
#include "http_base.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace bst {
    class http_server
    {
    private:
        /* data */
        bool stop_server_;
        std::shared_ptr<bst::context> svr_ctx_;
    public:
        http_server(/* args */)
        {
            stop_server_ = false;
            svr_ctx_ = base::server_ctx();
        }
        ~http_server(){}
        void init(const std::string &address, const unsigned short port, const int threads)
        {
            // Initialize the server
            svr_ctx_->set<int>("port",port);
            svr_ctx_->set<std::string>("address",address);
            svr_ctx_->set<int>("threads",threads);
             // The io_context is required for all I/O
            base::set_io_ctx(threads);
            // Set the default values
            svr_ctx_->set<int>("session_timeout", 3600);
            svr_ctx_->set<int>("request_timeout", 600);
            svr_ctx_->set<int>("max_connections", 10000);  
            svr_ctx_->set<int>("connection_timeout", 30);
            svr_ctx_->set<int>("request_timeout", 30);
            svr_ctx_->set<int>("max_requests", 600000);
            svr_ctx_->set<size_t>("max_request_body_size", 100*1024*1024);
        }
        // user can get and set the context
        // then use the context on the request_handler
        std::shared_ptr<bst::context> get_global()
        {
            return svr_ctx_->get_global();
        }
        void set_session_timeout(int seconds)
        {
            svr_ctx_->set<int>("session_timeout",seconds);
        }
        void set_request_timeout(int seconds)
        {
            svr_ctx_->set<int>("request_timeout",seconds);
        }
        void set_max_connections(int max) {
            svr_ctx_->set<int>("max_connections", max);
        }
        void set_connection_timeout(int seconds) {
            svr_ctx_->set<int>("connection_timeout", seconds);
        }
        void set_max_requests(int max) {
            svr_ctx_->set<int>("max_requests", max);
        }
        void set_max_request_body_size(size_t max) {
            svr_ctx_->set<size_t>("max_request_body_size", max);
        }
        // Accepts incoming connections and launches the sessions
        void run_server()
        {
            std::string address = svr_ctx_->get<std::string>("address").value();
            unsigned short port = svr_ctx_->get<int>("port").value();
            int threads = svr_ctx_->get<int>("threads").value();
            auto const boost_address = net::ip::make_address(address);

            // Spawn a listening port using co_spawn
            net::io_context &ioc = *base::get_io_ctx();
            net::co_spawn(ioc,
                request_connection::run_accept(ioc, tcp::endpoint{boost_address, port}, svr_ctx_),
                [](std::exception_ptr ex) {
                    if (ex) {
                        try {
                            std::rethrow_exception(ex);
                        } catch (const std::exception& e) {
                            fail(e, "run_server");
                        }
                    }
                });

            // Run the I/O service on the requested number of threads
            std::vector<std::thread> v;
            v.reserve(threads);
            for (auto i = 0; i < threads; i++)
                v.emplace_back([&ioc] { ioc.run(); });

            // If we get here then the server is stopping
            for (auto& t : v)
            {
                t.join();
            }
            ioc.stop();
        }
        void stop_server()
        {
            // stop the server
            stop_server_ = true;
            request_connection::stop_server();
        }
    };
} // namespace bst