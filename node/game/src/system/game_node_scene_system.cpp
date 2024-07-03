#include "game_node_scene_system.h"

#include "ue5navmesh/Public/Detour/DetourNavMesh.h"

#include "muduo/base/Logging.h"

#include "mainscene_config.h"
#include "scene_config.h"

#include "game_node.h"
#include "network/message_system.h"
#include "service/centre_scene_service.h"
#include "system/player_scene_system.h"
#include "system/recast_system.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage.h"

#include "component_proto/player_comp.pb.h"
#include "constants_proto/node.pb.h"

void GameNodeSceneSystem::LoadAllMainSceneNavBin()
{
    auto& config_all = mainscene_config::GetSingleton().all();
    for (auto& it : config_all.data())
    {
        const auto scene_nav_ptr = std::make_shared<SceneNavPtr::element_type>();
        scene_nav_ptr->p_nav_  = std::make_unique<SceneNav::DtNavMeshPtr::element_type>();
        scene_nav_ptr->p_nav_query_ = std::make_unique<SceneNav::DtNavMeshQueryPtr::element_type>();
        RecastSystem::LoadNavMesh(it.nav_bin_file().c_str(), scene_nav_ptr->p_nav_.get());
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

void GameNodeSceneSystem::CreateScene(CreateGameNodeSceneParam& p)
{
    ScenesSystem::CreateScene2GameNode(p);
    //init scene 
    if (const auto p_scene_row = get_scene_conf(p.scene_info.scene_confid());
        nullptr == p_scene_row)
    {
        LOG_ERROR << "scene config null" << p.scene_info.scene_confid();
        return;
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
