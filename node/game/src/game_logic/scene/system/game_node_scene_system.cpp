#include "game_node_scene_system.h"

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
#include "game_logic/scene/comp/grid.h"
#include "constants/scene_constants.h"
#include "comp/scene_comp.h"
#include "constants/dt_crowd.h"

#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/constants/node.pb.h"

void GameNodeSceneSystem::LoadAllMainSceneNavBin()
{
	auto& configAll = mainscene_config::GetSingleton().all();
	for (auto& item : configAll.data())
	{
		auto navIt = tlsGame.sceneNav.emplace(item.id(), SceneNav{});
		if (!navIt.second)
		{
			LOG_ERROR << "Failed to load scene navigation: " << item.id();
			continue;
		}
		auto& nav = navIt.first->second;
		RecastSystem::LoadNavMesh(item.nav_bin_file().c_str(), &nav.navMesh);
		nav.navQuery.init(&nav.navMesh, kMaxMeshQueryNodes);
	}
}

void GameNodeSceneSystem::InitNodeScene()
{
	if (!(gGameNode->GetNodeType() == eGameNodeType::kMainSceneNode ||
		gGameNode->GetNodeType() == eGameNodeType::kMainSceneCrossNode))
	{
		return;
	}

	const auto& mainSceneConf = mainscene_config::GetSingleton().all();
	for (auto& item : mainSceneConf.data())
	{
		CreateGameNodeSceneParam params{ .node = entt::entity{gGameNode->GetNodeId()} };
		params.sceneInfo.set_scene_confid(item.id());
		SceneSystem::CreateScene2GameNode(params);
	}
}

void GameNodeSceneSystem::EnterScene(const EnterSceneParam& param)
{
	SceneSystem::EnterScene(param);

	if (tls.registry.any_of<Player>(param.enter))
	{
		PlayerSceneSystem::OnEnterScene(param.enter, param.scene);
	}
}

void GameNodeSceneSystem::LeaveScene(entt::entity leaver)
{
	SceneSystem::LeaveScene({ .leaver = leaver });
}

void GameNodeSceneSystem::RegisterSceneToCentre(entt::entity scene)
{
	const auto sceneInfo = tls.sceneRegistry.try_get<SceneInfo>(scene);
	if (!sceneInfo)
	{
		return;
	}

	RegisterSceneRequest request;
	request.set_game_node_id(gGameNode->GetNodeId());
	request.mutable_scenes_info()->Add()->CopyFrom(*sceneInfo);

	BroadCastToCentre(CentreSceneServiceRegisterSceneMsgId, request);
}

void GameNodeSceneSystem::RegisterSceneToCentre()
{
	RegisterSceneRequest request;
	request.set_game_node_id(gGameNode->GetNodeId());

	for (auto&& [entity, sceneInfo] : tls.sceneRegistry.view<SceneInfo>().each())
	{
		request.mutable_scenes_info()->Add()->CopyFrom(sceneInfo);
	}

	BroadCastToCentre(CentreSceneServiceRegisterSceneMsgId, request);
}

void GameNodeSceneSystem::OnSceneCreateHandler(const OnSceneCreate& message)
{
	entt::entity scene = entt::to_entity(message.entity());
	tls.sceneRegistry.emplace<SceneGridList>(scene);

	auto& sceneInfo = tls.sceneRegistry.get<SceneInfo>(scene);
	if (tlsGame.sceneNav.contains(sceneInfo.scene_confid()))
	{
		// Auto-generated crowd handling
		// auto& dtCrowd = tls.sceneRegistry.emplace<dtCrowd>(scene);
		// dtCrowd.init(1000, kAgentRadius, &tls_game.sceneNav_[sceneInfo.sceneConfid()].navMesh);
	}
}

void GameNodeSceneSystem::AfterEnterSceneHandler(const AfterEnterScene& message)
{
	// Placeholder for future implementations
}

void GameNodeSceneSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{
	// Placeholder for future implementations
}
