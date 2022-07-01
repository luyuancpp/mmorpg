#pragma once
#include "entt/src/entt/entity/registry.hpp"

#include "src/game_logic/comp/scene_comp.h"


struct CreateSceneP
{
    uint32_t scene_confid_{ 0 };
};

struct CreateSceneWithGuidP : public CreateSceneP
{
    Guid scene_id{kInvalidGuid};
};

struct CreateGsSceneP
{
    entt::entity node_{ entt::null };
    uint32_t scene_confid_{ 0 };
};

struct MoveServerScene2ServerSceneP
{
    entt::entity from_server_{ entt::null };
    entt::entity to_server_{ entt::null };
};

struct DestroySceneParam
{
    entt::entity scene_{ entt::null };
    entt::entity server_{ entt::null };
};

struct DestroyServerParam
{
    entt::entity server_{ entt::null };
};

entt::entity& scenes_entity();

entt::entity CreateMainSceneNode();
void AddMainSceneNodeCompnent(entt::entity e);

