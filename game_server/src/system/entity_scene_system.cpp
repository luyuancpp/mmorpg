#include "entity_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/scene/servernode_system.h"
#include "src/game_logic/scene/scene.h"
#include "src/system/player_scene_system.h"

#include "component_proto/player_comp.pb.h"

EntitySceneSystem g_entity_scene_system;

void EntitySceneSystem::EnterScene(const EnterSceneParam& param)
{
    auto enterer = param.enterer_;
    ScenesSystem::GetSingleton().EnterScene(param);

    if (registry.any_of<Player>(enterer))
    {
        PlayerSceneSystem::OnEnterScene(enterer, param.scene_);
    }    
    else
    {

    }
}

void EntitySceneSystem::LeaveScene(entt::entity ent)
{
    LeaveSceneParam leave;
    leave.leaver_ = ent;
    ScenesSystem::GetSingleton().LeaveScene(leave);
}
