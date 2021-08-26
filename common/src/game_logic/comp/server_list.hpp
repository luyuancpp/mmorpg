#ifndef COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_
#define COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_

#include <memory>

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

namespace common
{
    class SceneConfigId
    {
    public:
        SceneConfigId(uint32_t scene_config_id) : scene_config_id_(scene_config_id) {}
        inline void set_scene_config_id(uint32_t scene_config_id) { scene_config_id_ = scene_config_id; }
        inline  uint32_t scene_config_id()const { return scene_config_id_; }
    private:
        uint32_t scene_config_id_{ 0 };
    };

    struct SceneEntityId
    {
        entt::entity scene_entity_{};
    };

    struct DungeonScene {};

    using SceneIds = std::unordered_set<entt::entity>;
    using SceneGroup = std::unordered_map<uint32_t, SceneIds>;
    using PlayerEntities = std::unordered_set<entt::entity>;

    struct Scenes
    {
        SceneGroup scenes_group_;
        SceneIds scenes_;
    };

    struct GameServerStatusNormal{};//game server Õý³£×´Ì¬
    struct GameServerMainTain{};//game server Î¬»¤×´Ì¬
    struct GameServerUpdate{};//game server ¸üÐÂ×´Ì¬
    struct GameServerCrash{};//±ÀÀ£×´Ì¬

    struct GameServerData
    {
        uint32_t server_id_{0};
        entt::entity server_entity_{};
        uint32_t player_size_{ 0 };
    };

    using GameServerDataPtr = std::shared_ptr<GameServerData>;

}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_