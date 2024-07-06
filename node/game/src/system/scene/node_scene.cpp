#include "node_scene.h"

#include "ue5navmesh/Public/Detour/DetourNavMesh.h"

#include "muduo/base/Logging.h"

#include "mainscene_config.h"
#include "scene_config.h"

#include "DetourCrowd/DetourCrowd.h"
#include "game_node.h"
#include "network/message_system.h"
#include "service/centre_scene_service.h"
#include "system/player/player_scene.h"
#include "system/recast.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "event_proto/scene_event.pb.h"
#include "comp/scene/grid.h"
#include "constants/scene.h"
#include "comp/scene.h"
#include "constants/dt_crowd.h"

#include "component_proto/player_comp.pb.h"
#include "constants_proto/node.pb.h"

void GameNodeSceneSystem::LoadAllMainSceneNavBin()
{
    auto& config_all = mainscene_config::GetSingleton().all();
    for (auto& it : config_all.data())
    {
        auto nav_it = tls_game.scene_nav_.emplace(it.id(), SceneNav{});
        if (!nav_it.second)
        {
            LOG_ERROR << "load scene nav err" << it.id();
            continue;
        }
        auto& nav = nav_it.first;
        RecastSystem::LoadNavMesh(it.nav_bin_file().c_str(), &nav->second.nav_mesh);
        nav->second.nav_query.init(&nav->second.nav_mesh, kMaxMeshQueryNodes);
    }
}

void GameNodeSceneSystem::InitNodeScene()
{
    if (!(g_game_node->GetNodeType() == eGameNodeType::kMainSceneNode || 
        g_game_node->GetNodeType() == eGameNodeType::kMainSceneCrossNode))
    {
        return;
    }
    const auto& main_scene_conf = mainscene_config::GetSingleton().all();
    for (auto& it : main_scene_conf.data())
    {
        CreateGameNodeSceneParam p{ .node_ = entt::entity{g_game_node->GetNodeId()} };
        p.scene_info.set_scene_confid(it.id());
        ScenesSystem::CreateScene2GameNode(p);
    }
}

void GameNodeSceneSystem::EnterScene(const EnterSceneParam& param)
{
    ScenesSystem::EnterScene(param);
    if (tls.registry.any_of<Player>(param.player_))
    {
        PlayerSceneSystem::OnEnterScene(param.player_, param.scene_);
    }
    else
    {
    }
}

void GameNodeSceneSystem::LeaveScene(entt::entity leaver)
{
    ScenesSystem::LeaveScene({.leaver_= leaver });
}

void GameNodeSceneSystem::RegisterSceneToCentre(entt::entity scene)
{
    const auto scene_info = tls.scene_registry.try_get<SceneInfo>(scene);
    if (nullptr == scene_info)
    {
        return;
    }
    RegisterSceneRequest rq;
    rq.set_game_node_id(g_game_node->GetNodeId());
    rq.mutable_scenes_info()->Add()->CopyFrom(*scene_info);
    BroadCastToCentre(CentreSceneServiceRegisterSceneMsgId, rq);
}

void GameNodeSceneSystem::RegisterSceneToCentre()
{
    RegisterSceneRequest rq;
    rq.set_game_node_id(g_game_node->GetNodeId());
    for (auto&& [e, scene_info] : tls.scene_registry.view<SceneInfo>().each())
    {
        rq.mutable_scenes_info()->Add()->CopyFrom(scene_info);
    }
    BroadCastToCentre(CentreSceneServiceRegisterSceneMsgId, rq);
}

void GameNodeSceneSystem::OnSceneCreateHandler(const OnSceneCreate& message)
{
    entt::entity scene = entt::to_entity(message.entity());
    tls.scene_registry.emplace<SceneGridList>(scene);

    auto& scene_info = tls.scene_registry.get<SceneInfo>(scene);
    if (tls_game.scene_nav_.contains(scene_info.scene_confid()))
    {
        //auto& dt_crowd = tls.scene_registry.emplace<dtCrowd>(scene);
        //dt_crowd.init(1000, kAgentRadius, &tls_game.scene_nav_[scene_info.scene_confid()].nav_mesh);
    }

}

void GameNodeSceneSystem::AfterEnterSceneHandler(const AfterEnterScene& message)
{
   
}

void GameNodeSceneSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{
   
}

