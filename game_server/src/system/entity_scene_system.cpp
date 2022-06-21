#include "entity_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/scene/servernode_system.h"
#include "src/game_logic/scene/scene.h"

EntitySceneSystem g_entity_scene_system;

void EntitySceneSystem::EnterScene(const EnterSceneParam& param)
{
    ScenesSystem::GetSingleton().EnterScene(param);
    OnAfterEnterScene(param.enterer_);
}

void EntitySceneSystem::OnAfterEnterScene(entt::entity ent)
{
}

void EntitySceneSystem::LeaveScene(entt::entity ent)
{
    OnBeforeLeaveScene(ent);
    LeaveSceneParam leave;
    leave.leaver_ = ent;
    ScenesSystem::GetSingleton().LeaveScene(leave);
}

void EntitySceneSystem::OnBeforeLeaveScene(entt::entity ent)
{
}
