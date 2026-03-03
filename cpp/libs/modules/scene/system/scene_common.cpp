#include "scene_common.h"
#include <core/utils/id/node_id_generator.h>
#include <muduo/base/Logging.h>
#include <registry_manager.h>
#include <node_context_manager.h>
#include <proto/common/base/common.pb.h>
#include <scene/comp/scene_node_comp.h>
#include <table/proto/tip/scene_error_tip.pb.h>
#include <table/proto/tip/common_error_tip.pb.h>
#include <proto/common/event/scene_event.pb.h>
#include <dispatcher_manager.h>
#include <scene/comp/scene_comp.h>
#include "scene_param.h"

static constexpr std::size_t kMaxScenePlayer1 = 1000;

thread_local TransientNode12BitCompositeIdGenerator  nodeSceneSequence; // Sequence for generating node IDs

NodeId SceneCommon::GetGameNodeIdFromGuid(uint64_t scene_id)
{
	return nodeSceneSequence.node_id(static_cast<NodeId>(scene_id));
}

entt::entity SceneCommon::GetSceneNodeEntityId(uint64_t scene_id)
{
	return entt::entity{ nodeSceneSequence.node_id(static_cast<NodeId>(scene_id)) };
}

void SceneCommon::SetSequenceNodeId(const uint32_t node_id) { nodeSceneSequence.set_node_id(node_id); }

void SceneCommon::ClearAllSceneData()
{
	LOG_TRACE << "Clearing scene system data";
	tlsRegistryManager.sceneRegistry.clear();
	tlsRegistryManager.actorRegistry.clear();
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
}

uint32_t SceneCommon::GenSceneGuid() {
	uint32_t sceneId = nodeSceneSequence.Generate();
	while (tlsRegistryManager.sceneRegistry.valid(entt::entity{ sceneId })) {
		sceneId = nodeSceneSequence.Generate();
	}
	LOG_INFO << "Generated new scene ID: " << sceneId;
	return sceneId;
}

bool SceneCommon::IsSceneEmpty() {
	bool isEmpty = tlsRegistryManager.sceneRegistry.storage<SceneInfoPBComponent>().empty();
	LOG_TRACE << "Scene registry empty: " << (isEmpty ? "true" : "false");
	return isEmpty;
}


// Get total number of scenes associated with a specific configuration ID
std::size_t SceneCommon::GetScenesSize(uint32_t sceneConfigId) {
	std::size_t sceneSize = 0;
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	for (auto node : registry.view<SceneRegistryComp>()) {
		auto& nodeSceneComp = registry.get<SceneRegistryComp>(node);
		sceneSize += nodeSceneComp.GetScenesByConfig(sceneConfigId).size();
	}
	LOG_TRACE << "Total scenes size for config ID " << sceneConfigId << ": " << sceneSize;
	return sceneSize;
}

// Get total number of scenes in the registry
std::size_t SceneCommon::GetScenesSize() {
	std::size_t totalScenes = tlsRegistryManager.sceneRegistry.storage<SceneInfoPBComponent>().size();
	LOG_TRACE << "Total scenes in the registry: " << totalScenes;
	return totalScenes;
}

bool SceneCommon::ConfigSceneListNotEmpty(uint32_t sceneConfigId) {
	auto& sceneNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	for (auto nodeEid : sceneNodeRegistry.view<SceneRegistryComp>()) {
		auto& nodeSceneComp = sceneNodeRegistry.get<SceneRegistryComp>(nodeEid);
		if (!nodeSceneComp.GetScenesByConfig(sceneConfigId).empty()) {
			LOG_TRACE << "Non-empty scene list found for config ID: " << sceneConfigId;
			return true;
		}
	}
	LOG_TRACE << "No non-empty scene list found for config ID: " << sceneConfigId;
	return false;
}

uint32_t SceneCommon::CheckPlayerEnterScene(const EnterSceneParam& param) {
	if (!tlsRegistryManager.sceneRegistry.valid(param.scene)) {
		LOG_ERROR << "Invalid scene entity when checking player enter scene - Scene ID: " << entt::to_integral(param.scene);
		return kInvalidEnterSceneParameters;
	}

	auto* sceneInfo = tlsRegistryManager.sceneRegistry.try_get<SceneInfoPBComponent>(param.scene);
	if (!sceneInfo) {
		LOG_ERROR << "SceneInfo not found when checking player enter scene - Scene ID: " << entt::to_integral(param.scene);
		return kInvalidEnterSceneParameters;
	}

	auto creatorId = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(param.enter);
	if (sceneInfo->creators().find(creatorId) == sceneInfo->creators().end()) {
		LOG_WARN << "Player cannot enter scene due to creator restriction - Scene ID: " << entt::to_integral(param.scene);
		return kCheckEnterSceneCreator;
	}

	return kSuccess;
}


uint32_t SceneCommon::HasSceneSlot(entt::entity scene) {
	auto& scenePlayers = tlsRegistryManager.sceneRegistry.get_or_emplace<ScenePlayers>(scene);

	if (scenePlayers.size() >= kMaxScenePlayer1) {
		LOG_WARN << "Scene player size limit exceeded - Scene ID: " << entt::to_integral(scene);
		return kEnterSceneNotFull;
	}

	auto* gsPlayerInfo = tlsRegistryManager.sceneRegistry.try_get<SceneNodePlayerStatsPtrPbComponent>(scene);
	if (!gsPlayerInfo) {
		LOG_ERROR << "GameNodePlayerInfoPtr not found for scene - Scene ID: " << entt::to_integral(scene);
		return kEnterSceneGsInfoNull;
	}

	if ((*gsPlayerInfo)->player_size() >= kMaxServerPlayerSize) {
		LOG_WARN << "Game node player size limit exceeded - Scene ID: " << entt::to_integral(scene);
		return kEnterSceneGsFull;
	}

	return kSuccess;
}


void SceneCommon::EnterScene(const EnterSceneParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering scene";
		return;
	}

	if (!tlsRegistryManager.sceneRegistry.valid(param.scene))
	{
		LOG_ERROR << "Invalid scene entity when entering scene - Scene ID: " << entt::to_integral(param.scene);
		return;
	}

	if (!tlsRegistryManager.actorRegistry.valid(param.enter))
	{
		LOG_ERROR << "Invalid player entity when entering scene - Player : " << entt::to_integral(param.enter);
		return;
	}

	auto& scenePlayers = tlsRegistryManager.sceneRegistry.get_or_emplace<ScenePlayers>(param.scene);
	scenePlayers.emplace(param.enter);
	if (tlsRegistryManager.actorRegistry.any_of<SceneEntityComp>(param.enter))
	{
		LOG_FATAL << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(param.enter);
	}
	tlsRegistryManager.actorRegistry.emplace<SceneEntityComp>(param.enter, param.scene);

	auto* gsPlayerInfo = tlsRegistryManager.sceneRegistry.try_get<SceneNodePlayerStatsPtrPbComponent>(param.scene);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() + 1);
	}

	AfterEnterScene afterEnterScene;
	afterEnterScene.set_entity(entt::to_integral(param.enter));
	dispatcher.trigger(afterEnterScene);

	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.enter)) {
		LOG_INFO << "Player entered scene - Player GUID: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(param.enter) << ", Scene ID: " << entt::to_integral(param.scene);
	}
}


// Leave a player from a scene
void SceneCommon::LeaveScene(const LeaveSceneParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when leaving scene";
		return;
	}

	if (!tlsRegistryManager.actorRegistry.valid(param.leaver)) {
		LOG_ERROR << "Invalid player entity when leaving scene - Player GUID: " << entt::to_integral(param.leaver);
		return;
	}

	auto sceneEntityComp = tlsRegistryManager.actorRegistry.try_get<SceneEntityComp>(param.leaver);
	if (nullptr == sceneEntityComp)
	{
		LOG_ERROR << "SceneEntityComp not found for player when leaving scene - Player : " << entt::to_integral(param.leaver);
		return;
	}

	auto sceneEntity = sceneEntityComp->sceneEntity;
	if (!tlsRegistryManager.sceneRegistry.valid(sceneEntity)) {
		LOG_ERROR << "Invalid scene entity when leaving scene - Player : " << entt::to_integral(param.leaver);
		return;
	}

	BeforeLeaveScene beforeLeaveScene;
	beforeLeaveScene.set_entity(entt::to_integral(param.leaver));
	dispatcher.trigger(beforeLeaveScene);

	auto& scenePlayers = tlsRegistryManager.sceneRegistry.get<ScenePlayers>(sceneEntity);
	scenePlayers.erase(param.leaver);
	tlsRegistryManager.actorRegistry.remove<SceneEntityComp>(param.leaver);

	auto* gsPlayerInfo = tlsRegistryManager.sceneRegistry.try_get<SceneNodePlayerStatsPtrPbComponent>(sceneEntity);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() - 1);
	}

	/*AfterLeaveScene afterLeaveScene;
	afterLeaveScene.set_entity(entt::to_integral(param.leaver));
	dispatcher.trigger(afterLeaveScene);*/

	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.leaver)) {
		LOG_INFO << "Player left scene - Player GUID: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(param.leaver) << ", Scene ID: " << entt::to_integral(sceneEntity);
	}
}

void SceneCommon::DestroyScene(const DestroySceneParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for destroying scene";
		return;
	}

	auto* pServerComp = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<SceneRegistryComp>(param.node);
	if (!pServerComp) {
		LOG_ERROR << "ServerComp not found for node";
		return;
	}

	auto* sceneInfo = tlsRegistryManager.sceneRegistry.try_get<SceneInfoPBComponent>(param.scene);
	if (!sceneInfo) {
		LOG_ERROR << "SceneInfo not found for scene";
		return;
	}

	OnSceneDestroyed sceneDestroyedEvent;
	sceneDestroyedEvent.set_entity(entt::to_integral(param.scene));
	dispatcher.trigger(sceneDestroyedEvent);

	pServerComp->RemoveScene(param.scene);

	LOG_INFO << "Destroyed scene with ID: " << sceneInfo->guid();
}


entt::entity SceneCommon::CreateSceneOnSceneNode(const CreateSceneOnNodeSceneParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for creating scene";
		return entt::null;
	}

	SceneInfoPBComponent sceneInfo(param.sceneInfo);
	if (sceneInfo.guid() <= 0) {
		sceneInfo.set_guid(SceneCommon::GenSceneGuid());
	}

	const auto scene = CreateEntityIfNotExists(tlsRegistryManager.sceneRegistry, entt::entity{ sceneInfo.guid() });
	if (scene == entt::null) {
		LOG_ERROR << "Failed to create scene in registry";
		return entt::null;
	}

	tlsRegistryManager.sceneRegistry.emplace<SceneInfoPBComponent>(scene, std::move(sceneInfo));
	tlsRegistryManager.sceneRegistry.emplace<ScenePlayers>(scene);

	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto* serverPlayerInfo = registry.try_get<SceneNodePlayerStatsPtrPbComponent>(param.node);
	if (serverPlayerInfo) {
		tlsRegistryManager.sceneRegistry.emplace<SceneNodePlayerStatsPtrPbComponent>(scene, *serverPlayerInfo);
	}

	auto* pServerComp = registry.try_get<SceneRegistryComp>(param.node);
	if (pServerComp) {
		pServerComp->AddScene(scene);
	}

	OnSceneCreated createSceneEvent;
	createSceneEvent.set_entity(entt::to_integral(scene));
	dispatcher.trigger(createSceneEvent);

	LOG_INFO << "Created new scene with ID: " << tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(scene).guid();
	return scene;
}