#ifndef COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_
#define COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

namespace common
{
    struct SceneConfigId
    {
        uint32_t scene_config_id_{ 0 };
    };

    using SceneIds = std::unordered_set<entt::entity>;
    using PlayerEntities = std::unordered_set<common::GameGuid>;

    struct SceneMap
    {
        using SceneGroup = std::unordered_map<uint32_t, SceneIds>;
        SceneGroup scenes_group_;
        SceneIds scenes_;
    };

    struct ScenePlayerList
    {
        PlayerEntities player_list_;
    };

    struct GameServerData
    {
        entt::entity server_id_{};
    };

    struct MainTainServer
    {

    };

    struct UpdateServer
    {

    };

    struct CrashServer
    {

    };

}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_