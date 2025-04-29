#pragma once
#include <boost/unordered_map.hpp>
#include <boost/any.hpp>
#include <shared_mutex>
#include <mutex>

namespace unordered = boost::unordered; // from <boost/unordered_map.hpp>

namespace bst
{
    class context
    {
    private:
        /* data */
        unordered::unordered_map<std::string, boost::any> ctx_;
        std::shared_mutex mtx_;
        std::shared_ptr<bst::context> sub_ctx_;
        std::shared_ptr<bst::context> g_ctx_;
    public:
        context(/* args */){};
        ~context(){clear();}

        // Get a value from the context
        template<typename T>
        std::optional<T> get(const std::string &key)
        {
            std::shared_lock<std::shared_mutex> lock(mtx_);
            auto it = ctx_.find(key);
            if (it == ctx_.end())
                return std::nullopt;
            try {
                return boost::any_cast<T>(it->second);
            } catch (const boost::bad_any_cast& e) {
                return std::nullopt;
            }
        }
     
        // Set a value in the context
        template<typename T>
        void set(const std::string &key, const T &value)
        {
            std::unique_lock<std::shared_mutex> lock(mtx_);
            ctx_[key] = value;
        }
        // Remove a value from the context
        void remove(const std::string &key)
        {
            std::unique_lock<std::shared_mutex> lock(mtx_);
            ctx_.erase(key);
        }
        // Remove a value from the context
        void clear()
        {
            std::unique_lock<std::shared_mutex> lock(mtx_);
            ctx_.clear();
        }
        //sub context
        std::shared_ptr<bst::context> get_sub()
        {
            if(!sub_ctx_) {
                sub_ctx_ = std::make_shared<bst::context>();
                sub_ctx_->g_ctx_ = g_ctx_;
            }
            return sub_ctx_;
        }
        //set the sub context
        void set_sub(std::shared_ptr<bst::context> sub_ctx)
        {
            if(!sub_ctx)
            {
                sub_ctx_ = std::make_shared<bst::context>();
                sub_ctx_ = sub_ctx;
                sub_ctx_->g_ctx_ = g_ctx_;
            }
            else
            {
                sub_ctx_ = sub_ctx;
                g_ctx_ = sub_ctx_->g_ctx_;
            }
        }

        //global context
        std::shared_ptr<bst::context> get_global()
        {
            if(!g_ctx_)     
                g_ctx_ =  std::make_shared<bst::context>();
            return g_ctx_;
        }
        
        //set the global context
        void set_global(std::shared_ptr<bst::context> g_ctx)
        {
            g_ctx_ = g_ctx;
        }
    };
} // namespace bst
