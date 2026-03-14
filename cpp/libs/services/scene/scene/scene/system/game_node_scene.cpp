#include "game_node_scene.h"

#include "table/code/mainscene_table.h"
#include "core/network/message_system.h"
#include "player/system/player_scene.h"
#include "scene/scene/comp/grid.h"
#include "proto/common/event/scene_event.pb.h"
#include "proto/common/base/node.pb.h"
#include "modules/scene/comp/scene_comp.h"
#include "modules/scene/system/scene_param.h"
#include "rpc/service_metadata/centre_scene_service_metadata.h"
#include "network/node_utils.h"
#include "network/node_message_utils.h"
#include "scene/scene/manager/scene_nav.h"
#include <threading/registry_manager.h>
#include <modules/scene/system/scene_common.h>

void GameNodeSceneSystem::InitializeNodeScenes() {
	if (!(GetNodeInfo().scene_node_type() == eSceneNodeType::kMainSceneNode ||
		GetNodeInfo().scene_node_type() == eSceneNodeType::kMainSceneCrossNode)) {
		return;
	}

	const auto& mainSceneConf = GetMainSceneAllTable();
	for (auto& item : mainSceneConf.data()) {
		CreateSceneOnNodeSceneParam params{ .node = entt::entity{GetNodeInfo().node_id()}};
		params.sceneInfo.set_scene_confid(item.id());
		SceneCommon::CreateSceneOnSceneNode(params);
	}
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

void GameNodeSceneSystem::HandleSceneCreation(const OnSceneCreated& message) {
	entt::entity scene = entt::to_entity(message.entity());

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