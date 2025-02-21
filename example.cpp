#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "server/http_server.hpp"
#include "server/request_handler.hpp"

//------------------------------------------------------------------------------
void Hello(http::request<http::string_body>& req, http::response<http::string_body>& res,bst::context& ctx)
{
    res.set(http::field::server, "Beast");
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = ctx.get<std::string>("hello");
    res.prepare_payload();
    //std::this_thread::sleep_for(std::chrono::seconds(3));
}

class HelloHandler : public std::enable_shared_from_this<HelloHandler>
{
    private:
    std::string other_;
    public:
    HelloHandler(/* args */){other_ = "hi ";};
    ~HelloHandler(){};
    void Hello1(http::request<http::string_body>& req, http::response<http::string_body>& res,bst::context& ctx)
    {
        res.set(http::field::server, "Beast");
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = other_ + ctx.get<std::string>("hello1");
        res.prepare_payload();
    }
    void Hello2(http::request<http::string_body>& req, http::response<http::string_body>& res,bst::context& ctx)
    {
        res.set(http::field::server, "Beast");
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = other_ + ctx.get<std::string>("hello2");
        res.prepare_payload();
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
            "    http-server-coro 0.0.0.0 8080 1\n";
        return EXIT_FAILURE;
    }

   
    auto const address = std::string(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const doc_root = std::make_shared<std::string>("./");
    auto const threads = std::max<int>(1, std::atoi(argv[3]));

    // Register the handler
    bst::request_handler::register_route("/hello", Hello);
    HelloHandler hh;
    bst::request_handler::register_route("/hello1", std::bind(&HelloHandler::Hello1,hh,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    bst::request_handler::register_route("/hello2", std::bind(&HelloHandler::Hello2,hh,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));

    bst::http_server server;
    //user can set the context with the server supported context
    //or set the data on other way
    server.get_context()->set<std::string>(std::string("hello"),std::string("hello!!!"));
    server.get_context()->set<std::string>(std::string("hello1"),std::string("hello1!!!"));
    server.get_context()->set<std::string>(std::string("hello2"),std::string("hello1!!!"));
    //
    server.run_server(address,port,threads);

    return EXIT_SUCCESS;
}
