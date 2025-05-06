#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include "request_session.hpp"
#include "context.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace bst {

    class request_connection {
    private:
        inline static bool stop_server_ = false;

    public:
        static net::awaitable<void> run_accept(
            net::io_context& ioc,
            tcp::endpoint endpoint,
            std::shared_ptr<bst::context> const& ctx)
        {
            beast::error_code ec;
            stop_server_ = false;

            // Open the acceptor
            tcp::acceptor acceptor(ioc);
            acceptor.open(endpoint.protocol(), ec);
            if (ec) co_return fail(ec, "open");

            // Allow address reuse
            acceptor.set_option(net::socket_base::reuse_address(true), ec);
            if (ec) co_return fail(ec, "set_option");

            // Bind to the server address
            acceptor.bind(endpoint, ec);
            if (ec) co_return fail(ec, "bind");

            // Start listening for connections
            acceptor.listen(net::socket_base::max_listen_connections, ec);
            if (ec) co_return fail(ec, "listen");

            // Accept incoming connections
            while (!stop_server_)
            {
                tcp::socket socket(ioc);
                co_await acceptor.async_accept(socket, net::use_awaitable);

                // Spawn a new session for each connection
                std::shared_ptr<request_session> session = std::make_shared<request_session>();
                net::co_spawn(
                    ioc,
                    session->run_session(std::move(socket),ctx),
                    net::detached);
            }
        }

        static void stop_server() {
            stop_server_ = true;
        }
    };

} // namespace bst