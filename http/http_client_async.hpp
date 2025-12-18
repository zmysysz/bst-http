#pragma once

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/url.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <chrono>
#include <deque>
#include <optional>
#include <thread>
#include <future>
#include <coroutine>
#include <boost/asio/steady_timer.hpp>
#include "http_base.hpp"

namespace bst {

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace net = asio;
using tcp = asio::ip::tcp;

struct request {
    std::string url;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    int version = 11;
    bool keep_alive = true;
};

struct response {
    int status_code = 0;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    int version = 11;
};

struct connection {
    std::shared_ptr<net::io_context> ioc;
    std::shared_ptr<beast::tcp_stream> stream;
    std::chrono::steady_clock::time_point last_used;
};

class connection_pool {
public:
    static connection_pool& instance() {
        static connection_pool inst;
        return inst;
    }

    void set_max_idle_connections(int num) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_idle_connections_ = num;
    }

    asio::awaitable<std::shared_ptr<connection>> async_get(const std::string& host, const std::string& port) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto key = host + ":" + port;
        auto& queue = pool_[key];
        while (!queue.empty()) {
            auto conn = queue.front();
            queue.pop_front();
            if (conn && conn->stream && conn->stream->socket().is_open()) {
                co_return conn;
            } else {
                shutdown_and_close(conn);
            }
        }
        co_return nullptr;
    }

    asio::awaitable<void> async_put(const std::string& host, const std::string& port, std::shared_ptr<connection> conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto key = host + ":" + port;
        auto& queue = pool_[key];
        if (queue.size() < max_idle_connections_) {
            conn->last_used = std::chrono::steady_clock::now();
            queue.push_back(conn);
        } else {
            shutdown_and_close(conn);
        }
        co_return;
    }

    void set_idle_timeout(int seconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        idle_timeout_ = seconds;
    }

private:
    void shutdown_and_close(std::shared_ptr<connection> conn) {
        if (conn && conn->stream && conn->stream->socket().is_open()) {
            boost::system::error_code ec;
            conn->stream->socket().shutdown(tcp::socket::shutdown_both, ec);
            conn->stream->socket().close(ec);
        }
    }

    void cleanup() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [key, queue] : pool_) {
            queue.erase(std::remove_if(queue.begin(), queue.end(), [&](std::shared_ptr<connection> conn) {
                return std::chrono::duration_cast<std::chrono::seconds>(now - conn->last_used).count() > idle_timeout_;
            }), queue.end());
        }
    }

    std::mutex mutex_;
    std::unordered_map<std::string, std::deque<std::shared_ptr<connection>>> pool_;
    int max_idle_connections_ = 50;
    int idle_timeout_ = 60;
};

class http_client_async {
public:
    http_client_async() {}
    ~http_client_async() {}
public:
    asio::awaitable<int> get(request& req, response& res) { 
        co_return co_await request_impl("GET", req, res); 
    }
    
    asio::awaitable<int> post(request& req, response& res) {
        co_return co_await request_impl("POST", req, res); 
    }
    
    asio::awaitable<int> put(request& req, response& res) { 
        co_return co_await request_impl("PUT", req, res); 
    }
    
    asio::awaitable<int> head(request& req, response& res) { 
        co_return co_await request_impl("HEAD", req, res); 
    }
    
    asio::awaitable<int> del(request& req, response& res) { 
        co_return co_await request_impl("DELETE", req, res); 
    }

    void set_connect_timeout(int seconds) { connect_timeout_ = seconds; }
    void set_request_timeout(int seconds) { request_timeout_ = seconds; }
    void set_response_timeout(int seconds) { response_timeout_ = seconds; }
    void set_max_request_size(int bytes) { max_request_size_ = bytes; }
    void set_max_response_size(int bytes) { max_response_size_ = bytes; }

    void set_idle_timeout(int seconds) {
        idle_timeout_ = seconds;
        connection_pool::instance().set_idle_timeout(seconds);
    }
    void set_max_redirects(int num) { max_redirects_ = num; }
    void set_max_retries(int num) { max_retries_ = num; }
    void set_max_idle_connections(int num) { connection_pool::instance().set_max_idle_connections(num); }

private:
    asio::awaitable<int> request_impl(const std::string& method, request& req, response& res, int redirect_count = 0, int retry_count = 0) {
        if (redirect_count > max_redirects_) co_return -4;
        if (retry_count > max_retries_) co_return -5;

        auto parsed = boost::urls::parse_uri(req.url);
        if (!parsed) co_return -1;
        auto uri = parsed.value();
        auto host = std::string(uri.host());
        auto port = uri.port().empty() ? (uri.scheme_id() == boost::urls::scheme::https ? "443" : "80") : std::string(uri.port());
        auto target = std::string(uri.encoded_path());
        if (uri.has_query()) target += "?" + std::string(uri.encoded_query());

        auto conn = co_await get_connection(host, port, req.keep_alive);
        if (!conn) co_return co_await request_impl(method, req, res, redirect_count, retry_count + 1);

        beast::flat_buffer buffer;
        http::request<http::string_body> http_req;
        http_req.method_string(method);
        http_req.version(req.version);
        http_req.target(target);
        http_req.body() = req.body;
        http_req.prepare_payload();
        http_req.set(http::field::host, host);
        if (req.keep_alive)
            http_req.keep_alive(true);
        for (const auto& [k, v] : req.headers)
            http_req.set(k, v);

        conn->stream->expires_after(std::chrono::seconds(request_timeout_));

        try {
            //set request size limit
            if (http_req.body().size() > static_cast<size_t>(max_request_size_)) {
                shutdown_and_close(conn);
                co_return -6;
            }
            co_await http::async_write(*conn->stream, http_req, asio::use_awaitable);
            http::response<http::string_body> http_res;
            http::request_parser<http::string_body> parser;
            //set limit on response size
            parser.body_limit(max_response_size_);
            conn->stream->expires_after(std::chrono::seconds(response_timeout_));
            co_await http::async_read(*conn->stream, buffer, http_res, asio::use_awaitable);

            res.status_code = http_res.result_int();
            res.version = http_res.version();
            res.body = http_res.body();
            for (auto const& field : http_res.base()) {
                res.headers[std::string(field.name_string())] = std::string(field.value());
            }

            if (res.status_code >= 300 && res.status_code < 400 && res.headers.count("Location")) {
                req.url = res.headers["Location"];
                shutdown_and_close(conn);
                co_return co_await request_impl(method, req, res, redirect_count + 1, 0);
            }

            if (req.keep_alive)
                co_await return_connection(host, port, conn);
            else
                shutdown_and_close(conn);

            co_return res.status_code;
        } catch (const beast::system_error& se) {
            if (se.code() != http::error::end_of_stream &&
                se.code() != beast::errc::connection_reset &&
                se.code() != beast::errc::operation_canceled &&
                se.code() != beast::errc::timed_out && 
                se.code() != beast::errc::network_down &&
                se.code() != beast::errc::stream_timeout &&
                se.code() != net::error::eof) {
                std::cerr << "Request system error: " << se.code().message() << std::endl;
            }
            shutdown_and_close(conn);
        } catch (const std::exception& ex) {
            std::cerr << "Request error exception: " << ex.what() << std::endl;
            shutdown_and_close(conn);
        }
        co_return co_await request_impl(method, req, res, redirect_count, retry_count + 1);
    }

    asio::awaitable<std::shared_ptr<connection>> get_connection(const std::string& host, const std::string& port, bool keep_alive) {
        if (keep_alive) {
            auto conn = co_await connection_pool::instance().async_get(host, port);
            if (conn && conn->stream && conn->stream->socket().is_open()) {
                co_return conn;
            }
            if (conn) {
                shutdown_and_close(conn);
            }
        }

        co_return co_await create_new_connection(host, port);
    }

    asio::awaitable<std::shared_ptr<connection>> create_new_connection(const std::string& host, const std::string& port) {
        try {
            //auto ioc = client_io_pool::instance().get_io_context();
            auto ioc = base::get_io_ctx()->get_executor();
            tcp::resolver resolver(ioc);
            auto results = co_await resolver.async_resolve(host, port, asio::use_awaitable);
            auto stream = std::make_shared<beast::tcp_stream>(ioc);
            co_await stream->async_connect(results, asio::use_awaitable);
            co_return std::make_shared<connection>(connection{nullptr,stream, std::chrono::steady_clock::now()});
        } catch (const std::exception& ex) {
            std::cerr << "Connection error: " << ex.what() << std::endl;
            co_return nullptr;
        } catch (...) {
            std::cerr << "Unknown connection error occurred." << std::endl;
            co_return nullptr;
        }
    }

    asio::awaitable<void> return_connection(const std::string& host, const std::string& port, std::shared_ptr<connection> conn) {
       co_return co_await connection_pool::instance().async_put(host, port, conn);
    }

    void shutdown_and_close(std::shared_ptr<connection> conn) {
        if (conn && conn->stream && conn->stream->socket().is_open()) {
            boost::system::error_code ec;
            conn->stream->socket().shutdown(tcp::socket::shutdown_both, ec);
            conn->stream->socket().close(ec);
        }
    }

    int connect_timeout_ = 5;
    int request_timeout_ = 60;
    int response_timeout_ = 60;
    int idle_timeout_ = 70;
    int max_redirects_ = 3;
    int max_retries_ = 2;
    int max_request_size_ = 10 * 1024 * 1024; // 10 MB
    int max_response_size_ = 10 * 1024 * 1024; // 10 MB
};

} // namespace bst