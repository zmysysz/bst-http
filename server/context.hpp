#pragma once
#include <boost/unordered_map.hpp>
#include <boost/any.hpp>
#include <shared_mutex>
#include <mutex>

namespace unordered = boost::unordered; // from <boost/unordered_map.hpp>

namespace bst
{
    class context;
    class context
    {
    private:
        /* data */
        unordered::unordered_map<std::string, boost::any> ctx_;
        std::shared_mutex mtx_;
    public:
        std::shared_ptr<bst::context> sub_ctx;
    public:
        context(/* args */){};
        ~context(){clear();}

        // Get a value from the context
        template<typename T>
        T get(const std::string &key)
        {
            std::shared_lock<std::shared_mutex> lock(mtx_);
            auto it = ctx_.find(key);
            if (it == ctx_.end())
                return T();
            return std::move(boost::any_cast<T>(it->second));
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
    };

   

} // namespace bst
