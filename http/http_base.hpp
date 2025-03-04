#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include "context.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>

namespace bst
{
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
