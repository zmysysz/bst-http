#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "../http/http_server.hpp"
#include "../http/http_client_async.hpp"
#include <boost/asio.hpp>

namespace net = boost::asio;
std::string test_domain;
//------------------------------------------------------------------------------
net::awaitable<void>  Hello(http::request<http::string_body>& req, http::response<http::string_body>& res,bst::context& ctx)
{
    res.set(http::field::server, "Beast");
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = ctx.get_global()->get<std::string>("hello").value() + " " + req.body();
    res.prepare_payload();
    co_return;
    //std::this_thread::sleep_for(std::chrono::seconds(3));
}

class HelloHandler : public std::enable_shared_from_this<HelloHandler>
{
    private:
    std::string other_;
    bst::http_client_async client_;
    public:
    HelloHandler(){
    }
    ~HelloHandler(){}
    net::awaitable<void>  Hello1(http::request<http::string_body>& req, http::response<http::string_body>& res,bst::context& ctx)
    {
        res.set(http::field::server, "Beast");
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = other_ + ctx.get_global()->get<std::string>("hello1").value();
        res.prepare_payload();
        co_return;
    }
    net::awaitable<void>  Hello2(http::request<http::string_body>& req, http::response<http::string_body>& res,bst::context& ctx)
    {
        res.set(http::field::server, "Beast");
        res.set(http::field::content_type, "text/html");
        //sleep 30ms
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        res.keep_alive(req.keep_alive());
        res.body() = other_ + ctx.get_global()->get<std::string>("hello2").value();

        res.prepare_payload();
        co_return;
    }

    net::awaitable<void> Hello3(http::request<http::string_body>& req, http::response<http::string_body>& res,bst::context& ctx)
    {
        res.set(http::field::server, "Beast");
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        bst::request client_req;
        client_req.url = "http://127.0.0.1:8090/hello1";
        client_req.keep_alive = true;
        bst::response client_res;
        int status = co_await client_.get(client_req ,client_res);
        res.body() = other_ + ctx.get_global()->get<std::string>("hello3").value() + client_res.body;
        res.prepare_payload();
        co_return;
    }
};

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr <<
            "Usage: http-server-coro <address> <port> <threads>\n" <<
            "Example:\n" <<
            "    http-server-coro 0.0.0.0 8090 1\n";
        return EXIT_FAILURE;
    }

    /*bst::http_client client;
    client.Get("http://www.google.com/abc/def?name=hello&age=20");*/
    auto const address = std::string(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const doc_root = std::make_shared<std::string>("./");
    auto const threads = std::max<int>(1, std::atoi(argv[3]));

    bst::http_server server;
    server.init(address,port,threads);
    // Register the handler
    bst::request_handler::register_route("/hello", Hello);
    std::shared_ptr<HelloHandler> hh = std::make_shared<HelloHandler>();
    bst::request_handler::register_route("/hello1", std::bind(&HelloHandler::Hello1,hh,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    bst::request_handler::register_route("/hello2", std::bind(&HelloHandler::Hello2,hh,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    bst::request_handler::register_route("/hello3", std::bind(&HelloHandler::Hello3,hh,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
   
    //user can set the context with the server supported context
    //or set the data on other way
    server.get_global()->set<std::string>(std::string("hello"),std::string("hello!!!"));
    server.get_global()->set<std::string>(std::string("hello1"),std::string("hello1!!!"));
    server.get_global()->set<std::string>(std::string("hello2"),std::string("hello2!!!"));
    server.get_global()->set<std::string>(std::string("hello3"),std::string("hello3!!!"));
    //
    server.run_server();

    return EXIT_SUCCESS;
}
