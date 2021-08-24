#ifndef COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_
#define COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_

#include <memory>

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

namespace common
{
    struct SceneConfigId
    {
        uint32_t scene_config_id_{ 0 };
    };

    struct DungeonScene {};

    using SceneIds = std::unordered_set<entt::entity>;
    using PlayerEntities = std::unordered_set<common::GameGuid>;

    struct Scenes
    {
        using SceneGroup = std::unordered_map<uint32_t, SceneIds>;
        SceneGroup scenes_group_;
        SceneIds scenes_;
    };

    struct ScenePlayerList
    {
        PlayerEntities player_list_;
    };

    enum class eGameServerStatus : std::uint8_t
    {
        eNormal,
        eMainTain,
        eUpdate,
        eCrash,
    };

    struct GameServerData
    {
        uint32_t server_id_{0};
        entt::entity server_entity_{};
        eGameServerStatus game_server_status_ = eGameServerStatus::eNormal;
    };

    using GameServerDataPtr = std::shared_ptr<GameServerData>;

}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_