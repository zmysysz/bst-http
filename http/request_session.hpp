#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include "request_handler.hpp"
#include "context.hpp"
#include "http_base.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace bst {
    class request_session : public std::enable_shared_from_this<request_session>
    {
    public:
        net::awaitable<void> run_session(
            beast::tcp_stream stream,
            std::shared_ptr<bst::context> const& ctx) {
            beast::flat_buffer buffer;

            try {
                stream.expires_after(std::chrono::seconds(
                    ctx->get<int>("session_timeout").value_or(600)));

                for (;;) {
                    // Check if socket is still valid
                    if (!stream.socket().is_open()) {
                        break;
                    }

                    // Read request
                    
                    http::request<http::string_body> req;
                    std::size_t bytes_transferred = 
                        co_await http::async_read(stream, buffer, req, net::use_awaitable);

                    if (bytes_transferred == 0) {
                        // No data read, connection might be closed
                        break;
                    }
                    std::shared_ptr<request_handler_impl> handler_impl = 
                    std::make_shared<request_handler_impl>();
                    // Handle request
                    auto res = co_await handler_impl->handle_request(ctx, std::move(req));

                    // Choose write method based on response size
                    if (res.body().size() < 1024 * 1024) {
                        // For small responses, use synchronous write
                        beast::error_code ec;
                        std::size_t bytes_written = http::write(stream, res, ec);
                        if (ec) {
                            fail(ec, "write");
                            break;
                        }
                        if (bytes_written == 0) {
                            break;
                        }
                    } else {
                        // For large responses, use asynchronous write
                        std::size_t bytes_written = 
                            co_await http::async_write(stream, res, net::use_awaitable);
                        if (bytes_written == 0) {
                            break;
                        }
                    }

                    // Check if connection should be closed
                    if (res.need_eof()) {
                        break;
                    }

                    // Clear buffer for next read
                    buffer.consume(buffer.size());
                }
            }
            catch (const beast::system_error& se) {
                if (se.code() != http::error::end_of_stream &&
                    se.code() != beast::errc::connection_reset &&
                    se.code() != beast::errc::operation_canceled) 
                {
                    fail(se.code(), "session");
                }
            }
            catch (const std::exception& e) {
                fail(e, "session");
            }

            // Ensure connection is properly closed
            if (stream.socket().is_open()) {
                // Try to send any pending data before closing
                beast::error_code shutdown_ec;
                stream.socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
                if (shutdown_ec && shutdown_ec != beast::errc::not_connected) {
                    fail(shutdown_ec, "shutdown");
                }
                stream.socket().close(shutdown_ec);
            }
        }
    };

} // namespace bst
