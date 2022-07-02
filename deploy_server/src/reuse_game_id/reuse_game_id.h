#ifndef DEPLOY_SERVER_SRC_REUSE_GAME_ID_REUSE_GAME_ID_H_
#define DEPLOY_SERVER_SRC_REUSE_GAME_ID_REUSE_GAME_ID_H_

#include "src/game_logic/reuse_id/reuse_id.h"

namespace deploy
{
    class ReuseGsId
    {
    public:
        using GsId = std::unordered_map<std::string, uint32_t>;
        using ScanGameId = std::unordered_set<uint32_t>;
        using FreeList = ::google::protobuf::Map<uint32_t, bool>;

        uint32_t size() const { return size_; }
        const FreeList& free_list() const { return free_list_; }
        uint32_t Max() const { return UINT16_MAX; }

        void set_size(uint32_t s) { size_ = s; }

        bool IsFull()const { return size_ >= Max(); }

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

        void OnDbLoadComplete();
        void OnDisConnect(const std::string& ip);
        void Destroy(uint32_t t) { free_list_.insert({ t, true }); }
        bool RemoveFree(uint32_t t) { return free_list_.erase(t) > 0; }        
        void Emplace(const std::string& ip, uint32_t id);
        void ScanOver();
    private:
        FreeList free_list_;
        uint32_t size_{ 0 };
        GsId gs_entities_;
        ScanGameId scan_node_ids_;

    };

}//namespace deploy

#endif//DEPLOY_SERVER_SRC_REUSE_GAME_ID_REUSE_GAME_ID_H_
