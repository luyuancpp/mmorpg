#include "game_node_scene_system.h"

#include "table/mainscene_table.h"
#include "core/network/message_system.h"
#include "player/system/player_scene_system.h"
#include "scene/scene/comp/grid_comp.h"
#include "proto/logic/event/scene_event.pb.h"
#include "proto/common/node.pb.h"
#include "scene/comp/scene_comp.h"
#include "scene/system/scene_system.h"
#include "service_info/centre_scene_service_info.h"
#include "network/node_utils.h"
#include "network/node_message_utils.h"
#include "scene/scene/mananger/scene_nav_mananger.h"

void GameNodeSceneSystem::InitializeNodeScenes() {
	if (!(GetNodeInfo().scene_node_type() == eSceneNodeType::kMainSceneNode ||
		GetNodeInfo().scene_node_type() == eSceneNodeType::kMainSceneCrossNode)) {
		return;
	}

	const auto& mainSceneConf = GetMainSceneAllTable();
	for (auto& item : mainSceneConf.data()) {
		CreateGameNodeSceneParam params{ .node = entt::entity{GetNodeInfo().node_id()}};
		params.sceneInfo.set_scene_confid(item.id());
		SceneUtil::CreateSceneToSceneNode(params);
	}
}

void GameNodeSceneSystem::RegisterSceneToAllCentre(entt::entity scene) {
	const auto sceneInfo = tlsRegistryManager.sceneRegistry.try_get<SceneInfoPBComponent>(scene);
	if (!sceneInfo) {
		return;
	}

	RegisterSceneRequest request;
	request.set_scene_node_id(GetNodeInfo().node_id());
	request.mutable_scenes_info()->Add()->CopyFrom(*sceneInfo);

	BroadcastToNodes(CentreSceneRegisterSceneMessageId, request, eNodeType::CentreNodeService);
}

void GameNodeSceneSystem::RegisterAllSceneToCentre(entt::entity centre)
{
	RegisterSceneRequest request;
	request.set_scene_node_id(GetNodeInfo().node_id());

	for (auto&& [entity, sceneInfo] : tlsRegistryManager.sceneRegistry.view<SceneInfoPBComponent>().each()) {
		request.mutable_scenes_info()->Add()->CopyFrom(sceneInfo);
	}

	LOG_INFO << "Registering all scenes to CentreNode: " << entt::to_integral(centre)
		<< ", " << request.DebugString();

	CallRemoteMethodOnClient(CentreSceneRegisterSceneMessageId, request, entt::to_integral(centre), eNodeType::CentreNodeService);
}

void GameNodeSceneSystem::HandleSceneCreation(const OnSceneCreate& message) {
	entt::entity scene = entt::to_entity(message.entity());
	tlsRegistryManager.sceneRegistry.emplace<SceneGridListComp>(scene);

	auto& sceneInfo = tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(scene);
	if (SceneNavManager::Instance().Contains(sceneInfo.scene_confid())) {
		// Auto-generated crowd handling
		// auto& dtCrowd = tlsRegistryManager.sceneRegistry.emplace<dtCrowd>(scene);
		// dtCrowd.init(1000, kAgentRadius, &tls_game.sceneNav_[sceneInfo.sceneConfid()].navMesh);
	}
}

void GameNodeSceneSystem::HandleAfterEnterSceneEvent(const AfterEnterScene& message) {
	// Placeholder for future implementations
}

void GameNodeSceneSystem::HandleBeforeLeaveSceneEvent(const BeforeLeaveScene& message) {
	// Placeholder for future implementations
}