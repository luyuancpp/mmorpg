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
    entt::entity scene_map_entity_{};
    entt::entity scene_entity_{};
    entt::entity server_entity_{};
    uint32_t op_{ 0 };
};

struct MoveServerScene2ServerParam
{
    entt::entity from_server_entity_{};
    entt::entity to_server_entity_{};
    uint32_t op_{ 0 };
};

struct DestroySceneParam
{
    entt::entity scene_entity_{};
    entt::entity scene_map_entity_{};
};

struct DestroyServerParam
{
    entt::entity server_entity_{};
    entt::entity scene_map_entity_{};
};

entt::entity MakeScenes(entt::registry& reg);

entt::entity MakeMainScene(entt::registry& reg, const MakeSceneParam& param);

entt::entity MakeGameServer(entt::registry& reg, const MakeGameServerParam& param);

entt::entity MakeScene2GameServer(entt::registry& reg, const MakeScene2GameServerParam& param);

void PutScene2GameServer(entt::registry& reg, const PutScene2GameServerParam& param);

void MoveServerScene2Server(entt::registry& reg, const MoveServerScene2ServerParam& param);

void DestroyScene(entt::registry& reg, const DestroySceneParam& param);

void DestroyServer(entt::registry& reg, const DestroyServerParam& param);

}//namespace master

#endif//MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_
