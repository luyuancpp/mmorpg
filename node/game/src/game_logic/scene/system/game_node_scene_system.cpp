#include "game_node_scene_system.h"

#include "ue5navmesh/Public/Detour/DetourNavMesh.h"
#include "muduo/base/Logging.h"

#include "mainscene_config.h"
#include "scene_config.h"

#include "DetourCrowd/DetourCrowd.h"
#include "game_node.h"
#include "game_logic/network/message_system.h"
#include "service/centre_scene_service.h"
#include "game_logic/player/util/player_util.h"
#include "game_logic/scene/util/recast_util.h"
#include "scene/util/scene_util.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "logic/event/scene_event.pb.h"
#include "game_logic/scene/comp/grid_comp.h"
#include "game_logic/scene/constants/nav_constants.h"
#include "scene/comp/scene_comp.h"

#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/constants/node.pb.h"

void GameNodeSceneUtil::LoadAllMainSceneNavBin()
{
	auto& configAll = mainscene_config::GetSingleton().all();
	for (auto& item : configAll.data())
	{
		auto navIt = tlsGame.sceneNav.emplace(item.id(), NavComp{});
		if (!navIt.second)
		{
			LOG_ERROR << "Failed to load scene navigation: " << item.id();
			continue;
		}
		auto& nav = navIt.first->second;
		RecastUtil::LoadNavMesh(item.nav_bin_file().c_str(), &nav.navMesh);
		nav.navQuery.init(&nav.navMesh, kMaxMeshQueryNodes);
	}
}

void GameNodeSceneUtil::InitNodeScene()
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
		SceneUtil::CreateScene2GameNode(params);
	}
}

void GameNodeSceneUtil::EnterScene(const EnterSceneParam& param)
{
	SceneUtil::EnterScene(param);

	if (tls.registry.any_of<Player>(param.enter))
	{
		PlayerSceneUtil::OnEnterScene(param.enter, param.scene);
	}
}

void GameNodeSceneUtil::LeaveScene(entt::entity leaver)
{
	SceneUtil::LeaveScene({ .leaver = leaver });
}

void GameNodeSceneUtil::RegisterSceneToCentre(entt::entity scene)
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

void GameNodeSceneUtil::RegisterSceneToCentre()
{
	RegisterSceneRequest request;
	request.set_game_node_id(gGameNode->GetNodeId());

	for (auto&& [entity, sceneInfo] : tls.sceneRegistry.view<SceneInfo>().each())
	{
		request.mutable_scenes_info()->Add()->CopyFrom(sceneInfo);
	}

	BroadCastToCentre(CentreSceneServiceRegisterSceneMsgId, request);
}

void GameNodeSceneUtil::OnSceneCreateHandler(const OnSceneCreate& message)
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

void GameNodeSceneUtil::AfterEnterSceneHandler(const AfterEnterScene& message)
{
	// Placeholder for future implementations
}

void GameNodeSceneUtil::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{
	// Placeholder for future implementations
}
