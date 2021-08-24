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
    uint32_t server_id_{};
    uint32_t op_{ 0 };
};

struct MakeScene2GameServerParam
{
    entt::entity scene_map_entity_{};
    entt::entity server_entity_{};
    uint32_t scene_config_id_{ 0 };
    uint32_t op_{ 0 };
};

struct PutScene2GameServerParam
{
    entt::entity scene_entity_{};
    entt::entity server_entity_{};
    uint32_t op_{ 0 };
};

struct RemoveSceneParam
{
    entt::entity scene_id_{};
};

entt::entity MakeScenes(entt::registry& reg);

entt::entity MakeMainScene(entt::registry& reg,
    const MakeSceneParam& param);

entt::entity MakeGameServer(entt::registry& reg,
    const MakeGameServerParam& param);

entt::entity MakeScene2GameServer(entt::registry& reg,
    const MakeScene2GameServerParam& param);

void PutScene2GameServer(entt::registry& reg,
    const PutScene2GameServerParam& param);

void DestroyScene(entt::registry& reg, const RemoveSceneParam& param);

}//namespace master

#endif//MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_
