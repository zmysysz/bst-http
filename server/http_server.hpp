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
            //make server context
            svr_ctx_ = std::make_shared<bst::context>();
            //make user context
            svr_ctx_->sub_ctx = std::make_shared<bst::context>();
            //defaute pragmas
            svr_ctx_->set<int>("session_timeout",3600);
            svr_ctx_->set<int>("request_timeout",3600);
        }
        ~http_server(){}

        // user can get and set the context
        // then use the context on the request_handler
        std::shared_ptr<bst::context> get_context()
        {
            return svr_ctx_->sub_ctx;
        }
        //
        void set_session_timeout(int seconds)
        {
            // Set the timeout
            svr_ctx_->set<int>("session_timeout",seconds);
        }
        //
        void set_request_timeout(int seconds)
        {
            // Set the timeout
            svr_ctx_->set<int>("request_timeout",seconds);
        }
        // Accepts incoming connections and launches the sessions
        void run_server(const std::string &address, const unsigned short port, const int threads)
        {
            svr_ctx_->set<int>("port",port);
            svr_ctx_->set<std::string>("address",address);
            svr_ctx_->set<int>("threads",threads);
             // The io_context is required for all I/O
            net::io_context ioc{threads};
            auto const boost_address = net::ip::make_address(address);
            // Spawn a listening port
            boost::asio::spawn(ioc,
                std::bind(
                    &request_connection::run_accept,
                    std::ref(ioc),
                    tcp::endpoint{boost_address, port},
                    svr_ctx_,
                    std::placeholders::_1));

            // Run the I/O service on the requested number of threads
            std::vector<std::thread> v;
            v.reserve(threads);
            for(auto i = 0; i < threads; i++)
                v.emplace_back([&ioc]{ioc.run();});
            while(!stop_server_)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            // If we get here then the server is stopping
            ioc.stop();
            for(auto& t : v)
                t.join();
        }
        void stop_server()
        {
            // stop the server
            stop_server_ = true;
        }
    };
} // namespace bst