#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <regex>
#include <iostream>
#include "context.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>

namespace bst
{
    static void fail(const beast::error_code &ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << std::endl;
    }
    static void fail(const std::exception& ex, char const* what)
    {
        std::cerr << what << ": " << ex.what() << std::endl;
    }
    static void fail(const std::string &what)
    {
        std::cerr << what << std::endl;
    }

    class util
    {
        public:
            util(){}
            ~util(){}
            static bool parse_url(const std::string& url, std::string& host, std::string& port, std::string& target) {
                std::regex url_regex(R"(^(http|https)://([^:/]+)(?::(\d+))?(/.*)?$)");
                std::smatch match;
                if (std::regex_match(url, match, url_regex)) {
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
