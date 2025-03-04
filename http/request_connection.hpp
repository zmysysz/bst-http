#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <iostream>
#include <boost/asio/spawn.hpp>
#include "request_session.hpp"
#include "context.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace bst {
    class request_connection
    {
    private:
        /* data */
    public:
        request_connection(/* args */){};
        ~request_connection(){};
        // Accepts incoming connections and launches the sessions
        static void run_accept(
            net::io_context& ioc,
            tcp::endpoint endpoint,
            std::shared_ptr<bst::context> const& ctx,
            net::yield_context yield)
        {
            beast::error_code ec;

            // Open the acceptor
            tcp::acceptor acceptor(ioc);
            acceptor.open(endpoint.protocol(), ec);
            if(ec)
                return fail(ec, "open");

            // Allow address reuse
            acceptor.set_option(net::socket_base::reuse_address(true), ec);
            if(ec)
                return fail(ec, "set_option");

            // Bind to the server address
            acceptor.bind(endpoint, ec);
            if(ec)
                return fail(ec, "bind");

            // Start listening for connections
            acceptor.listen(net::socket_base::max_listen_connections, ec);
            if(ec)
                return fail(ec, "listen");

            for(;;)
            {
                tcp::socket socket(ioc);
                acceptor.async_accept(socket, yield[ec]);
                if(ec)
                    fail(ec, "accept");
                else
                    boost::asio::spawn(
                        acceptor.get_executor(),
                        std::bind(
                            request_session::run_session,
                            beast::tcp_stream(std::move(socket)),
                            ctx,
                            std::placeholders::_1));
            }
        }
    };
} // namespace bst