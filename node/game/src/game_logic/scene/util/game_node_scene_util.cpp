#include "game_node_scene_util.h"

#include "game_node.h"
#include "mainscene_config.h"
#include "game_logic/core/network/message_system.h"
#include "game_logic/player/util/player_scene_system.h"
#include "game_logic/scene/comp/grid_comp.h"
#include "logic/event/scene_event.pb.h"
#include "proto/logic/constants/node.pb.h"
#include "scene/comp/scene_comp.h"
#include "scene/util/scene_util.h"
#include "service_info/centre_scene_service_info.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"


void GameNodeSceneUtil::InitializeNodeScenes() {
	if (!(gGameNode->GetNodeType() == eGameNodeType::kMainSceneNode ||
		gGameNode->GetNodeType() == eGameNodeType::kMainSceneCrossNode)) {
		return;
	}

	const auto& mainSceneConf = GetMainSceneAllTable();
	for (auto& item : mainSceneConf.data()) {
		CreateGameNodeSceneParam params{ .node = entt::entity{gGameNode->GetNodeId()} };
		params.sceneInfo.set_scene_confid(item.id());
		SceneUtil::CreateScene2GameNode(params);
	}
}

void GameNodeSceneUtil::RegisterSceneToCentre(entt::entity scene) {
	const auto sceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComponent>(scene);
	if (!sceneInfo) {
		return;
	}

	RegisterSceneRequest request;
	request.set_game_node_id(gGameNode->GetNodeId());
	request.mutable_scenes_info()->Add()->CopyFrom(*sceneInfo);

	BroadCastToCentre(CentreSceneServiceRegisterSceneMessageId, request);
}

void GameNodeSceneUtil::RegisterSceneToCentre() {
	RegisterSceneRequest request;
	request.set_game_node_id(gGameNode->GetNodeId());

	for (auto&& [entity, sceneInfo] : tls.sceneRegistry.view<SceneInfoPBComponent>().each()) {
		request.mutable_scenes_info()->Add()->CopyFrom(sceneInfo);
	}

	BroadCastToCentre(CentreSceneServiceRegisterSceneMessageId, request);
}

void GameNodeSceneUtil::HandleSceneCreation(const OnSceneCreate& message) {
	entt::entity scene = entt::to_entity(message.entity());
	tls.sceneRegistry.emplace<SceneGridListComp>(scene);

	auto& sceneInfo = tls.sceneRegistry.get<SceneInfoPBComponent>(scene);
	if (tlsGame.sceneNav.contains(sceneInfo.scene_confid())) {
		// Auto-generated crowd handling
		// auto& dtCrowd = tls.sceneRegistry.emplace<dtCrowd>(scene);
		// dtCrowd.init(1000, kAgentRadius, &tls_game.sceneNav_[sceneInfo.sceneConfid()].navMesh);
	}
}

void GameNodeSceneUtil::HandleAfterEnterSceneEvent(const AfterEnterScene& message) {
	// Placeholder for future implementations
}

void GameNodeSceneUtil::HandleBeforeLeaveSceneEvent(const BeforeLeaveScene& message) {
	// Placeholder for future implementations
}