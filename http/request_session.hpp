#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <iostream>
#include <boost/asio/spawn.hpp>
#include "request_handler.hpp"
#include  "context.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace bst {
    static void fail(beast::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << std::endl;
    }

    class request_timer
    {
        // This class is used to keep the session alive and to time it out.
    private:
        /* data */
        beast::tcp_stream& stream_;
        beast::error_code &ec_;
        net::steady_timer timeout_timer_;
    public:
        request_timer(beast::tcp_stream& stream, beast::error_code &ec)
            : stream_(stream)
              , ec_(ec)
              , timeout_timer_(stream.get_executor())
        {
        }
        ~request_timer() {timeout_timer_.cancel();}
        void set_and_wait(int seconds)
        {
            timeout_timer_.expires_after(std::chrono::seconds(seconds));
            timeout_timer_.async_wait([&] (beast::error_code ec) {
                if (!ec) {
                    stream_.socket().shutdown(tcp::socket::shutdown_both,ec_);
                    stream_.close();
                    fail(ec, "time_out");
                }
            });
        }
        void cancel()
        {
            timeout_timer_.cancel();
        }
    };

    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    struct send_lambda
    {
        beast::tcp_stream& stream_;
        bool& close_;
        beast::error_code& ec_;
        net::yield_context yield_;

        send_lambda(
            beast::tcp_stream& stream,
            bool& close,
            beast::error_code& ec,
            net::yield_context yield)
            : stream_(stream)
            , close_(close)
            , ec_(ec)
            , yield_(yield)
        {
        }

        template<bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // Determine if we should close the connection after
            close_ = msg.need_eof();

            // We need the serializer here because the serializer requires
            // a non-const file_body, and the message oriented version of
            // http::write only works with const messages.
            http::serializer<isRequest, Body, Fields> sr{msg};
            http::async_write(stream_, sr, yield_[ec_]);
        }
    };

    class request_session
    {
    private:
        /* data */
    public:
        request_session(/* args */){};
        ~request_session(){};
       
        // Handles an HTTP server connection
        static void run_session(
            beast::tcp_stream& stream,
            std::shared_ptr<bst::context> const& ctx,
            net::yield_context yield)
        {
            bool close = false;
            beast::error_code ec;

            // This buffer is required to persist across reads
            beast::flat_buffer buffer;

            // This timer is used to keep the session alive
            //request_timer timer(stream,ec);

            // This lambda is used to send messages
            send_lambda lambda{stream, close, ec, yield};

            //
            int session_timeout = ctx->get<int>("session_timeout");
            int request_timeout = ctx->get<int>("request_timeout");

            for(;;)
            {
                // Set the session timeout.
                stream.expires_after(std::chrono::seconds(session_timeout));
                //Set request timeout
                //timer.set_and_wait(request_timeout);
                // Read a request
                http::request<http::string_body> req;
                http::async_read(stream, buffer, req, yield[ec]);
                if (ec == net::error::operation_aborted) {
                    fail(ec, "read");
                    break;
                }
                else if(ec == http::error::end_of_stream) {
                    // This means they closed the connection
                    break;
                }
                else if(ec) {
                    fail(ec, "read");
                    break;
                }
                // Send the response
                request_handler::handle_request(ctx, std::move(req), lambda,yield);
                if(ec) {
                    fail(ec, "write");
                    break;
                }
                if(close) {
                    // This means we should close the connection, usually because
                    // the response indicated the "Connection: close" semantic.
                    break;
                }
                //timer.cancel();
            }

            // Send a TCP shutdown
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
            stream.close();
            // At this point the connection is closed gracefully
        }
    };
} // namespace bst
