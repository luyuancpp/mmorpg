#ifndef COMMON_SRC_GAME_COLOGIC_REUSE_ID_REUSR_ID_H_
#define COMMON_SRC_GAME_COLOGIC_REUSE_ID_REUSR_ID_H_

#include <set>
#include <unordered_set>

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

        void Destroy(T t) { free_list_.emplace(t); }

    private:
        FreeList free_list_;
        T size_{0};
    };

}//namespace common

#endif//COMMON_SRC_GAME_COLOGIC_REUSE_ID_REUSR_ID_H_
