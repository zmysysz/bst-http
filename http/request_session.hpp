#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include "request_handler.hpp"
#include "context.hpp"
#include "http_base.hpp"
#include <boost/algorithm/string.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace bst {
#define SOCKET_BUFFER_SIZE 8192
#define LARGE_REQUEST_BODY_SIZE 5*1024*1024

    class request_session : public std::enable_shared_from_this<request_session>
    {
    public:
        explicit request_session()
        {
        }
        ~request_session() = default;

        net::awaitable<void> run_session(tcp::socket &&socket,std::shared_ptr<bst::context> const& ctx) {
            beast::flat_buffer buffer;
            beast::tcp_stream stream(std::move(socket));
        
            // Set the timeout for the session
            stream.expires_after(std::chrono::seconds(
                ctx->get<int>("session_timeout").value_or(600)));
            size_t max_request_body_size = ctx->get<size_t>("max_request_body_size").value_or(100 * 1000 * 1000);
            // Lambda to send 500 response if stream is valid
            auto send_500 = [&](beast::tcp_stream& s) -> net::awaitable<void> {
                if (s.socket().is_open()) {
                    http::response<http::string_body> res{http::status::internal_server_error, 11};
                    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(http::field::content_type, "text/plain");
                    res.body() = "Internal server error";
                    res.prepare_payload();
                    co_await http::async_write(s, res, net::use_awaitable);
                }
                co_return;
            };
            bool be_error = false;
            for (;;) {
                try {
                    // Check if socket is still valid
                    if (!stream.socket().is_open()) {
                        break;
                    }

                    // Read request
                    http::request_parser<http::string_body> parser;
                    parser.body_limit(max_request_body_size);
                    
                    std::size_t header_bytes = co_await http::async_read_header(stream, buffer, parser, net::use_awaitable);
                    if (header_bytes == 0) {
                        // No data read, connection might be closed
                        break;
                    }
                    // read the body
                    auto method = parser.get().method();
                    auto content_length = parser.content_length().value_or(0);
                    bool is_large = false;
                    if (method == http::verb::post || method == http::verb::put) {
                        is_large = content_length > LARGE_REQUEST_BODY_SIZE ? true : false;
                        // if header include 100-continue, we need to send 100-continue
                        if (parser.get().find(http::field::expect) != parser.get().end() && boost::iequals(parser.get()[http::field::expect], "100-continue"))  {
                            http::response<http::empty_body> res{http::status::continue_, parser.get().version()};
                            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                            res.set(http::field::content_length, "0");
                            co_await http::async_write(stream, res, net::use_awaitable);
                        }
                        
                        beast::error_code ec;
                        //comsume any remaining body data in the buffer
                        if(buffer.size() > 0) {
                            std::size_t parsed = parser.put(buffer.data(), ec);
                            buffer.consume(parsed);
                        }
                        //read the remaining body data from the stream
                        while (!parser.is_done()) {
                            std::size_t n,parsed;
                            if(is_large)
                                n = stream.read_some(buffer.prepare(SOCKET_BUFFER_SIZE), ec);
                            else
                                n = co_await stream.async_read_some(buffer.prepare(SOCKET_BUFFER_SIZE), net::use_awaitable);
                            if(n > 0) {
                                buffer.commit(n);
                                parsed = parser.put(buffer.data(), ec);
                            }
                            if (ec) {
                                break;
                            }
                            buffer.consume(parsed);
                        }
                        if (ec) {
                            throw beast::system_error{ ec };
                            break;
                        }
                        // compare the received body size with content-length
                        if(content_length != parser.get().body().size()) {
                            throw std::runtime_error("Content-Length mismatch, content-length: " + std::to_string(content_length) + ", body size: " + std::to_string(parser.get().body().size()));
                            break;
                        }
                    }
                    // Create request context
                    auto req = std::make_shared<http::request<http::string_body>>(std::move(parser.get()));
                    std::shared_ptr<bst::request_context> req_ctx = std::make_shared<bst::request_context>(stream);
                    req_ctx->set_sub(ctx);
                    req_ctx->req = req;

                    // Handle request
                    auto handler_impl = std::make_shared<request_handler_impl>();
                    int status = co_await handler_impl->handle_request(req_ctx, req);
                    
                    // Prepare response
                    if (req_ctx->is_auto_response()) {
                        co_await req_ctx->response();
                    }     
                    // Clear buffer for next read
                    buffer.consume(buffer.size());
                    continue;
                }
                catch (const beast::system_error& se) {
                    if (se.code() != http::error::end_of_stream &&
                        se.code() != beast::errc::connection_reset &&
                        se.code() != beast::errc::operation_canceled &&
                        se.code() != beast::errc::timed_out && 
                        se.code() != beast::errc::network_down &&
                        se.code() != beast::errc::stream_timeout &&
                        se.code() != net::error::eof)
                    {
                        fail(se, "session system_error");
                        be_error = true;
                    }
                }
                catch (const std::exception& e) {
                    fail(e, "session exception");
                    be_error = true;
                }
                if(be_error) {
                    // If an error occurred, send a 500 response and break the loop
                    co_await send_500(stream);
                }
                break;
            }
            // Ensure connection is properly closed
            response_sender::close(stream);
        }
    };

} // namespace bst
