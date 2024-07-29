#include "node_scene.h"

#include "ue5navmesh/Public/Detour/DetourNavMesh.h"

#include "muduo/base/Logging.h"

#include "mainscene_config.h"
#include "scene_config.h"

#include "DetourCrowd/DetourCrowd.h"
#include "game_node.h"
#include "system/network/message_system.h"
#include "service/centre_scene_service.h"
#include "system/player/player_scene.h"
#include "system/recast.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "logic/event/scene_event.pb.h"
#include "comp/scene/grid.h"
#include "constants/scene.h"
#include "comp/scene.h"
#include "constants/dt_crowd.h"

#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/constants/node.pb.h"

void GameNodeSceneSystem::LoadAllMainSceneNavBin()
{
    auto& config_all = mainscene_config::GetSingleton().all();
    for (auto& it : config_all.data())
    {
        auto nav_it = tlsGame.sceneNav.emplace(it.id(), SceneNav{});
        if (!nav_it.second)
        {
            LOG_ERROR << "load scene nav err" << it.id();
            continue;
        }
        auto& nav = nav_it.first;
        RecastSystem::LoadNavMesh(it.nav_bin_file().c_str(), &nav->second.navMesh);
        nav->second.navQuery.init(&nav->second.navMesh, kMaxMeshQueryNodes);
    }
}

void GameNodeSceneSystem::InitNodeScene()
{
    if (!(gGameNode->GetNodeType() == eGameNodeType::kMainSceneNode || 
        gGameNode->GetNodeType() == eGameNodeType::kMainSceneCrossNode))
    {
        return;
    }
    const auto& main_scene_conf = mainscene_config::GetSingleton().all();
    for (auto& it : main_scene_conf.data())
    {
        CreateGameNodeSceneParam p{ .node = entt::entity{gGameNode->GetNodeId()} };
        p.sceneInfo.set_scene_confid(it.id());
        SceneSystem::CreateScene2GameNode(p);
    }
}

void GameNodeSceneSystem::EnterScene(const EnterSceneParam& param)
{
    SceneSystem::EnterScene(param);
    if (tls.registry.any_of<Player>(param.enter))
    {
        PlayerSceneSystem::OnEnterScene(param.enter, param.scene);
    }
    else
    {
    }
}

void GameNodeSceneSystem::LeaveScene(entt::entity leaver)
{
    SceneSystem::LeaveScene({.leaver= leaver });
}

void GameNodeSceneSystem::RegisterSceneToCentre(entt::entity scene)
{
    const auto scene_info = tls.sceneRegistry.try_get<SceneInfo>(scene);
    if (nullptr == scene_info)
    {
        return;
    }
    RegisterSceneRequest rq;
    rq.set_game_node_id(gGameNode->GetNodeId());
    rq.mutable_scenes_info()->Add()->CopyFrom(*scene_info);
    BroadCastToCentre(CentreSceneServiceRegisterSceneMsgId, rq);
}

void GameNodeSceneSystem::RegisterSceneToCentre()
{
    RegisterSceneRequest rq;
    rq.set_game_node_id(gGameNode->GetNodeId());
    for (auto&& [e, scene_info] : tls.sceneRegistry.view<SceneInfo>().each())
    {
        rq.mutable_scenes_info()->Add()->CopyFrom(scene_info);
    }
    BroadCastToCentre(CentreSceneServiceRegisterSceneMsgId, rq);
}

void GameNodeSceneSystem::OnSceneCreateHandler(const OnSceneCreate& message)
{
    entt::entity scene = entt::to_entity(message.entity());
    tls.sceneRegistry.emplace<SceneGridList>(scene);

    auto& scene_info = tls.sceneRegistry.get<SceneInfo>(scene);
    if (tlsGame.sceneNav.contains(scene_info.scene_confid()))
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

