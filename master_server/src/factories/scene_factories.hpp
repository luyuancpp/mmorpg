#ifndef MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_
#define MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_logic/comp/gs_scene_comp.hpp"

namespace master
{
struct MakeMainSceneP
{
    uint32_t scene_confid_{ 0 };
    uint32_t op_{ 0 };
};

struct MakeGSParam
{
    uint32_t node_id_{};
    uint32_t op_{ 0 };
};

struct MakeGSSceneP
{
    entt::entity server_entity_{ entt::null };
    uint32_t scene_confid_{ 0 };
    uint32_t op_{ 0 };
};

struct PutScene2GSParam
{
    entt::entity scene_entity_{ entt::null };
    entt::entity server_entity_{ entt::null };
    uint32_t op_{ 0 };
};

struct MoveServerScene2ServerSceneP
{
    entt::entity from_server_entity_{ entt::null };
    entt::entity to_server_entity_{ entt::null };
    uint32_t op_{ 0 };
};

struct DestroySceneParam
{
    entt::entity scene_entity_{ entt::null };
};

struct DestroyServerParam
{
    entt::entity server_entity_{ entt::null };
};

entt::entity& scenes_entity();

void MakeScenes();

entt::entity MakeMainScene(entt::registry& reg, const MakeMainSceneP& param);

entt::entity MakeMainSceneNode(entt::registry& reg, const MakeGSParam& param);
void AddMainSceneNodeCompnent(entt::entity e, const MakeGSParam& param);

}//namespace master

#endif//MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_
