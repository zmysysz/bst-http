#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/unordered_map.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>

namespace bst
{
    class http_client
    {
    private:
        /* data */
        int connect_time_out_;
        int request_time_out_;
        int response_time_out_;
        int idle_time_out_;
        int max_redirects_;
        int max_retries_;
    public:
        http_client()
        :connect_time_out_(3)
        ,request_time_out_(100)
        ,response_time_out_(100)
        ,idle_time_out_(300)
        ,max_redirects_(5)
        ,max_retries_(3)
        {}
        ~http_client(){}

        void Get(const std::string &url)
        {
            //http::request<http::string_body> req{http::verb::get, url, 11};
        }
        void Post(const std::string &url)
        {
            //http::request<http::string_body> req{http::verb::post, url, 11};
        }
        void Put(const std::string &url)
        {
            //http::request<http::string_body> req{http::verb::put, url, 11};
        }
        void Delete(const std::string &url)
        {
            //http::request<http::string_body> req{http::verb::delete_, url, 11};
        }
        void Head(const std::string &url)
        {
            //http::request<http::string_body> req{http::verb::head, url, 11};
        }
        void Options(const std::string &url)
        {
            //http::request<http::string_body> req{http::verb::options, url, 11};
        }
        void Patch(const std::string &url)
        {
            //http::request<http::string_body> req{http::verb::patch, url, 11};
        }
        void Connect(const std::string &url)
        {
            //http::request<http::string_body> req{http::verb::connect, url, 11};
        }
        void Trace(const std::string &url)
        {
            //http::request<http::string_body> req{http::verb::trace, url, 11};
        }
        void set_connect_time_out(int seconds){connect_time_out_ = seconds;}
        void set_request_time_out(int seconds){request_time_out_ = seconds;}
        void set_response_time_out(int seconds){response_time_out_ = seconds;}
        void set_idle_time_out(int seconds){idle_time_out_ = seconds;}
        void set_max_redirects(int num){max_redirects_ = num;}
        void set_max_retries(int num){max_retries_ = num;}
    };
} // namespace bst      