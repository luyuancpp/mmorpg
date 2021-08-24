#ifndef MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_
#define MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_logic/comp/server_list.hpp"

namespace master
{
struct MakeSceneParam
{
    entt::entity scene_map_entity_{};
    uint32_t scene_config_id_{ 0 };
    uint32_t op_{ 0 };
};

struct MakeGameServerParam
{
    entt::entity scene_map_entity_{};
    uint32_t server_id_{};
    common::SceneIds scenes_id_;
    uint32_t op_{ 0 };
};

struct MakeScene2GameServerParam
{
    entt::entity scene_map_entity_{};
    entt::entity server_entity_{};
    uint32_t scene_config_id_{ 0 };
    uint32_t op_{ 0 };
};

entt::entity MakeMainSceneMap(entt::registry& reg);

entt::entity MakeMainScene(entt::registry& reg,
    const MakeSceneParam& param);

entt::entity MakeGameServer(entt::registry& reg,
    const MakeGameServerParam& param);

entt::entity MakeScene2GameServer(entt::registry& reg,
    const MakeScene2GameServerParam& param);

entt::entity MakeDungeonScene(entt::registry& reg,
    const MakeSceneParam& param);
}//namespace master

#endif//MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_
