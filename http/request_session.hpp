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
        explicit request_session()
        {
        }
        ~request_session() = default;

        net::awaitable<void> run_session(tcp::socket &&socket,
            std::shared_ptr<bst::context> const& ctx) {
            beast::flat_buffer buffer;
            beast::tcp_stream stream(std::move(socket));
            try {
                // Set the timeout for the session
                stream.expires_after(std::chrono::seconds(
                    ctx->get<int>("session_timeout").value_or(600)));

                for (;;) {
                    // Check if socket is still valid
                    if (!stream.socket().is_open()) {
                        break;
                    }

                    // Read request
                    auto handler_impl = std::make_shared<request_handler_impl>();
                    auto req = std::make_shared<http::request<http::string_body>>();
                    
                    std::size_t bytes_transferred = 
                        co_await http::async_read(stream, buffer, *req, net::use_awaitable);

                    if (bytes_transferred == 0) {
                        // No data read, connection might be closed
                        break;
                    }
                    std::shared_ptr<bst::request_context> req_ctx = std::make_shared<bst::request_context>(stream);
                    req_ctx->set_sub(ctx);
                    req_ctx->req = req;

                    // Handle request
                    int status = co_await handler_impl->handle_request(req_ctx, req);
                    // Prepare response
                    if (req_ctx->is_auto_response()) {
                        co_await req_ctx->response();
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
                fail(e, "session1");
            }
            // Ensure connection is properly closed
            response_sender::close(stream);
        }
    };

} // namespace bst
