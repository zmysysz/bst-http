#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include "context.hpp"
#include <boost/asio.hpp>
#include <boost/stacktrace.hpp>
#include <iomanip>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = net::ip::tcp;

namespace bst
{
    static std::string get_time(std::time_t ts = 0, const std::string& format = "%Y-%m-%d %H:%M:%S.%f") {
        if(ts == 0) ts = std::time(nullptr);
        std::tm tm{};
        // use thread-safe localtime_r when available
    #if defined(_POSIX_THREADS)
        if (localtime_r(&ts, &tm) == nullptr) return "";
    #else
        auto tptr = std::localtime(&ts);
        if (!tptr) return "";
        tm = *tptr;
    #endif
        std::ostringstream oss;
        oss << std::put_time(&tm, format.c_str());
        return oss.str();
    }

    static void log_stacktrace(int skip = 0)
    {
        auto strace = boost::stacktrace::stacktrace();
        for(std::size_t i = skip; i < strace.size(); ++i) {
            std::cerr << strace[i] << std::endl;
        }
    }

    static void fail(const beast::system_error& se, char const* what)
    {
        std::cerr << get_time() << " " << what << ": " << se.code().message() << " --> " << se.what() << std::endl;
        std::cerr << "Stack trace:" << std::endl;
        log_stacktrace(2);
    }
    static void fail(const beast::error_code &ec, char const* what)
    {
        std::cerr << get_time() << " " << what << ": " << ec.message() << std::endl;
        std::cerr << "Stack trace:" << std::endl;
        log_stacktrace(2);
    }
    static void fail(const std::exception& ex, char const* what)
    {
        std::cerr << get_time() << " " << what << ": " << ex.what() << std::endl;
        std::cerr << "Stack trace:" << std::endl;
        log_stacktrace(2);
    }
    static void fail(const std::string& msg)
    {
        std::cerr << get_time() << " " << msg << std::endl;
        std::cerr << "Stack trace:" << std::endl;
        log_stacktrace(2);
    }


    class util
    {
    public:
        util(){}
        ~util(){}
        static bool parse_url(const std::string& url, std::string& host, std::string& port, std::string& target) {
            static boost::regex url_regex(R"(^(http|https)://([^:/]+)(?::(\d+))?(/.*)?$)");
            boost::smatch match;
            if (boost::regex_match(url, match, url_regex)) {
                std::string scheme = match[1];
                host = match[2];
                port = match[3].str().empty() ? (scheme == std::string("https") ? std::string("443") : std::string("80")) : match[3];
                target = match[4].str().empty() ? std::string("/") : match[4];
            } else {
                fail("Invalid URL format, " + url);
                return false;
            }
            return true;
        }

        static bool parse_request(boost::beast::string_view request_uri, 
                std::string& path, std::map<std::string, 
                std::string>& params) {
            if(request_uri.empty()) {
                fail("Request URI is empty");
                return false;
            }
            size_t pos = request_uri.find('?');
            if (pos == boost::beast::string_view::npos) {
                path = std::string(request_uri);
                return true;
            }
            path = std::string(request_uri.substr(0, pos));
            boost::beast::string_view query = request_uri.substr(pos + 1);
            while (!query.empty()) {
                size_t amp = query.find('&');
                boost::beast::string_view pair = query.substr(0, amp);
                size_t eq = pair.find('=');

                if (eq != boost::beast::string_view::npos) {
                    // Split into key and value
                    boost::beast::string_view key = pair.substr(0, eq);
                    boost::beast::string_view value = pair.substr(eq + 1);
                    params.emplace(std::string(key), std::string(value));
                } else {
                    // No '=', treat the whole thing as a key with empty value
                    params.emplace(std::string(pair), std::string());
                }
                // Advance to the next parameter, if any
                if (amp == boost::beast::string_view::npos)
                    break;
                query.remove_prefix(amp + 1);
            }
            return true;
        }
    };
    
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
    class response_sender {
        public:
        response_sender() = default;
        ~response_sender() = default;
        template<class Body>
        static void prepare_response(std::shared_ptr<http::request<http::string_body>> req, std::shared_ptr<http::response<Body>> res)
        {
            //if req require close,we close
            // Set common response headers
            res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res->keep_alive(req->keep_alive());
            res->prepare_payload();
        }
        // Handle request and prepare response
        template<class Body>
        static net::awaitable<bool> write(beast::tcp_stream& stream, std::shared_ptr<http::response<Body>> res) {
            try {
                // Choose write method based on response size
                if (res->body().size() < 1024 * 1024) {
                    // For small responses, use synchronous write
                    beast::error_code ec;
                    std::size_t bytes_written = http::write(stream, *res, ec);
                    if (ec) {
                        throw beast::system_error(ec);
                    }
                    if (bytes_written == 0) {
                        throw std::runtime_error("write 0 bytes");
                    }
                } else {
                    // For large responses, use asynchronous write
                    std::size_t bytes_written = 
                        co_await http::async_write(stream, *res, net::use_awaitable);
                    if (bytes_written == 0) {
                        throw std::runtime_error("write 0 bytes");
                    }
                }
            }
            catch (const beast::system_error& se) {
                if (se.code() != http::error::end_of_stream &&
                    se.code() != beast::errc::connection_reset &&
                    se.code() != beast::errc::operation_canceled) 
                {
                    fail(se.code(), "write");
                    co_return false;
                }
            }
            catch (const std::exception& e) {
                fail(e, "write1");
                co_return false;
            }
            co_return true;    
        }

        template<class Body>
        static bool keep_open(std::shared_ptr<http::response<Body>> res) {
            if (!res 
                || res->need_eof() 
                || res->keep_alive() == false) {
                return false;
            }
            return true;
        }

        // Close the stream
        static void close(beast::tcp_stream& stream) {
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

    class base
    {
    private:
        /* data */
        inline static std::shared_ptr<net::io_context> io_ctx_;
        inline static std::shared_ptr<bst::context> svr_ctx_;
    private:
         //Set the io_context
         static void set_io_ctx(int num_threads)
         {
             if (!io_ctx_)
                 io_ctx_ = std::make_shared<net::io_context>(num_threads);
         }
         //set the context
        static std::shared_ptr<bst::context> server_ctx()
        {
            if(!svr_ctx_)
            {
                svr_ctx_ = std::make_shared<bst::context>();
                svr_ctx_->get_global();
                //defaute pragmas
                svr_ctx_->set<int>("session_timeout", 3600);
                svr_ctx_->set<int>("request_timeout", 3600);
            }
            return svr_ctx_;
        }
        friend class http_server;
        friend class http_client;
    public: 
        base(/* args */){};
        ~base(){};
        // Print an error message and return a failure code
        static void error_print(beast::error_code ec, char const* what)
        {
            std::cerr << what << ": " << ec.message() << std::endl;
        }
       
        // Get the io_context
        static std::shared_ptr<net::io_context> get_io_ctx()
        {
            if(io_ctx_)
                return io_ctx_;
            return nullptr;
        }
    };
} // namespace bst
