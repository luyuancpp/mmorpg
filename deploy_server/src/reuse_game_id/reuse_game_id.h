#ifndef DEPLOY_SERVER_SRC_REUSE_GAME_ID_REUSE_GAME_ID_H_
#define DEPLOY_SERVER_SRC_REUSE_GAME_ID_REUSE_GAME_ID_H_

#include "src/game_logic/reuse_id/reuse_id.h"

namespace deploy
{
    class ReuseGameServerId : public common::ReuseId< uint32_t, ::google::protobuf::Map<uint32_t, bool>, UINT16_MAX>
    {
    public:
        using GameServerId = std::unordered_map<std::string, uint32_t>;
        using ScanGameId = std::unordered_set<uint32_t>;

        std::size_t free_list_size() { return free_list().size(); }
        bool IsScanEmpty() { return scan_game_server_ids_.empty(); }

        void OnDbLoadComplete();

        void Emplace(const std::string& ip, uint32_t id);

        void OnDisConnect(const std::string& ip);

        void EraseScanEmpty(uint32_t id);

        void ScanOver();
    private:
        GameServerId game_entities_;
        ScanGameId scan_game_server_ids_;

    };

}//namespace deploy

#endif//DEPLOY_SERVER_SRC_REUSE_GAME_ID_REUSE_GAME_ID_H_
