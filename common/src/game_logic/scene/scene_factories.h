#pragma once
#include "entt/src/entt/entity/registry.hpp"

#include "src/game_logic/comp/gs_scene_comp.h"


struct MakeSceneP
{
    uint32_t scene_confid_{ 0 };
};

struct MakeSceneWithGuidP : public MakeSceneP
{
    Guid scene_id{kInvalidGuid};
};

struct MakeGSParam
{
    uint32_t node_id_{};
    uint32_t op_{ 0 };
};

struct MakeGSSceneP
{
    entt::entity server_{ entt::null };
    uint32_t scene_confid_{ 0 };
    uint32_t op_{ 0 };
};

struct PutScene2GSParam
{
    entt::entity scene_{ entt::null };
    entt::entity server_{ entt::null };
    uint32_t op_{ 0 };
};

struct MoveServerScene2ServerSceneP
{
    entt::entity from_server_{ entt::null };
    entt::entity to_server_{ entt::null };
    uint32_t op_{ 0 };
};

struct DestroySceneParam
{
    entt::entity scene_{ entt::null };
};

struct DestroyServerParam
{
    entt::entity server_{ entt::null };
};

entt::entity& scenes_entity();

void MakeScenes();


entt::entity MakeMainSceneNode(entt::registry& reg, const MakeGSParam& param);
void AddMainSceneNodeCompnent(entt::entity e, const MakeGSParam& param);

