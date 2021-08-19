#ifndef COMMON_SRC_GAME_COLOGIC_REUSE_ID_REUSR_ID_H_
#define COMMON_SRC_GAME_COLOGIC_REUSE_ID_REUSR_ID_H_

#include <set>
#include <unordered_set>

#include "google/protobuf/map.h"

namespace common
{
    template<typename T, typename FreeList, T tmax>
    class ReuseId
    {
    public:

        T Max() { return tmax; }

        T Create() 
        {
            if (free_list_.empty())
            {
                return size_++;
            }
            auto it = free_list_.begin();
            T t = *it;
            free_list_.erase(it);
            return t;
        }

        T CreateNoReuse()
        {
            return size_++;
        }

        void Destroy(T t) { free_list_.emplace(t); }
        bool RemoveFree(T t) { free_list_.erase(t); }
    private:
        FreeList free_list_;
        T size_{0};
    };

    template<> 
    class ReuseId<uint32_t, ::google::protobuf::Map<uint32_t, bool>, UINT16_MAX>
    {
    public:
        using FreeList = ::google::protobuf::Map<uint32_t, bool>;

        uint32_t size() const { return size_; }
        const FreeList& free_list() const { return free_list_; }

        void set_size(uint32_t s) { size_ = s; }
        void set_free_list(const FreeList& free_list) { free_list_ = free_list; }

        uint32_t Max() { return UINT16_MAX; }

        uint32_t Create()
        {
            if (free_list_.empty())
            {
                return size_++;
            }
            auto it = free_list_.begin();
            uint32_t t = it->first;
            free_list_.erase(it);
            return t;
        }

        uint32_t CreateNoReuse()
        {
            return size_++;
        }

        void Destroy(uint32_t t) { free_list_.insert({ t, true}); }
        bool RemoveFree(uint32_t t) { return free_list_.erase(t) > 0; }
    private:
        FreeList free_list_;
        uint32_t size_{ 0 };
    };

}//namespace common

#endif//COMMON_SRC_GAME_COLOGIC_REUSE_ID_REUSR_ID_H_
