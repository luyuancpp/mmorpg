#pragma once

#include <set>
#include <unordered_set>

#include "google/protobuf/map.h"


template<typename T, typename FreeList, T tmax>
class ReuseId
{
public:

    T Max() const { return tmax; }

    bool IsFull()const { return size_ >= Max(); }

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

    void Destroy(T t) { free_list_.emplace(t); }
    void RemoveFree(T t) { free_list_.erase(t); }
private:
    FreeList free_list_;
    T size_{0};
};

template<> 
class ReuseId<uint32_t, ::google::protobuf::Map<uint32_t, bool>, UINT16_MAX>
{
public:

};


