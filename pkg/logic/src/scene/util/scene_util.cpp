#include "scene_util.h"
#include "muduo/base/Logging.h"

#include "scene/comp/scene_comp.h"
#include "node/constants/node_constants.h"
#include "thread_local/storage.h"
#include "pbc/scene_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "proto/logic/component/game_node_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"

#include <ranges> // Only if using C++20 ranges

// Constants
static constexpr std::size_t kMaxScenePlayer = 1000;

// Type alias
using GameNodePlayerInfoPtrPBComponent = std::shared_ptr<GameNodePlayerInfoPBComponent>;

// Static function
void SetServerSequenceNodeId(uint32_t nodeId) {
	LOG_TRACE << "Setting server sequence node ID to: " << nodeId;
	SceneUtil::SetSequenceNodeId(nodeId);
}

// Function to add main scene node components
void AddMainSceneNodeComponent(entt::registry& reg, const entt::entity node) {
	LOG_TRACE << "Adding main scene node components for entity: " << entt::to_integral(node);
	reg.emplace<MainSceneNode>(node);
	reg.emplace<NodeSceneComp>(node);
	reg.emplace<GameNodePlayerInfoPtrPBComponent>(node, std::make_shared<GameNodePlayerInfoPBComponent>());
}

// ScenesSystem implementation
SceneUtil::SceneUtil() {
	LOG_TRACE << "ScenesSystem constructor called";
	Clear();
}

SceneUtil::~SceneUtil() {
	LOG_TRACE << "ScenesSystem destructor called";
	Clear();
}

NodeId SceneUtil::GetGameNodeId(uint64_t scene_id)
{
	return nodeSequence.node_id(static_cast<NodeId>(scene_id));
}

entt::entity SceneUtil::get_game_node_eid(uint64_t scene_id)
{
	return entt::entity{ nodeSequence.node_id(static_cast<NodeId>(scene_id)) };
}

void SceneUtil::SetSequenceNodeId(const uint32_t node_id) { nodeSequence.set_node_id(node_id); }

void SceneUtil::Clear() {
	LOG_TRACE << "Clearing scene system data";
	tls.sceneRegistry.clear();
	tls.registry.clear();
	tls.gameNodeRegistry.clear();
}

// Get game node ID associated with a scene entity
NodeId SceneUtil::GetGameNodeId(entt::entity scene) {
	auto* sceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComp>(scene);
	if (sceneInfo) {
		return GetGameNodeId(sceneInfo->guid());
	}
	else {
		LOG_ERROR << "SceneInfo not found for entity: " << entt::to_integral(scene);
		return kInvalidNodeId;
	}
}

// Generate unique scene ID
uint32_t SceneUtil::GenSceneGuid() {
	uint32_t sceneId = nodeSequence.Generate();
	while (tls.sceneRegistry.valid(entt::entity{ sceneId })) {
		sceneId = nodeSequence.Generate();
	}
	LOG_INFO << "Generated new scene ID: " << sceneId;
	return sceneId;
}

// Get total number of scenes associated with a specific configuration ID
std::size_t SceneUtil::GetScenesSize(uint32_t sceneConfigId) {
	std::size_t sceneSize = 0;
	for (auto node : tls.gameNodeRegistry.view<NodeSceneComp>()) {
		auto& nodeSceneComp = tls.gameNodeRegistry.get<NodeSceneComp>(node);
		sceneSize += nodeSceneComp.GetScenesByConfig(sceneConfigId).size();
	}
	LOG_TRACE << "Total scenes size for config ID " << sceneConfigId << ": " << sceneSize;
	return sceneSize;
}

// Get total number of scenes in the registry
std::size_t SceneUtil::GetScenesSize() {
	std::size_t totalScenes = tls.sceneRegistry.storage<SceneInfoPBComp>().size();
	LOG_TRACE << "Total scenes in the registry: " << totalScenes;
	return totalScenes;
}

// Check if scene registry is empty
bool SceneUtil::IsSceneEmpty() {
	bool isEmpty = tls.sceneRegistry.storage<SceneInfoPBComp>().empty();
	LOG_TRACE << "Scene registry empty: " << (isEmpty ? "true" : "false");
	return isEmpty;
}

// Check if there are non-empty scene lists for a specific configuration
bool SceneUtil::ConfigSceneListNotEmpty(uint32_t sceneConfigId) {
	for (auto nodeEid : tls.gameNodeRegistry.view<NodeSceneComp>()) {
		auto& nodeSceneComp = tls.gameNodeRegistry.get<NodeSceneComp>(nodeEid);
		if (!nodeSceneComp.GetScenesByConfig(sceneConfigId).empty()) {
			LOG_TRACE << "Non-empty scene list found for config ID: " << sceneConfigId;
			return true;
		}
	}
	LOG_TRACE << "No non-empty scene list found for config ID: " << sceneConfigId;
	return false;
}

// Create a new scene associated with a game node
entt::entity SceneUtil::CreateScene2GameNode(const CreateGameNodeSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for creating scene";
		return entt::null;
	}

	SceneInfoPBComp sceneInfo(param.sceneInfo);
	if (sceneInfo.guid() <= 0) {
		sceneInfo.set_guid(GenSceneGuid());
	}

	const auto scene = tls.sceneRegistry.create(entt::entity{ sceneInfo.guid() });
	if (scene == entt::null) {
		LOG_ERROR << "Failed to create scene in registry";
		return entt::null;
	}

	tls.sceneRegistry.emplace<SceneInfoPBComp>(scene, std::move(sceneInfo));
	tls.sceneRegistry.emplace<ScenePlayers>(scene);

	auto* serverPlayerInfo = tls.gameNodeRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(param.node);
	if (serverPlayerInfo) {
		tls.sceneRegistry.emplace<GameNodePlayerInfoPtrPBComponent>(scene, *serverPlayerInfo);
	}

	auto* pServerComp = tls.gameNodeRegistry.try_get<NodeSceneComp>(param.node);
	if (pServerComp) {
		pServerComp->AddScene(scene);
	}

	OnSceneCreate createSceneEvent;
	createSceneEvent.set_entity(entt::to_integral(scene));
	tls.dispatcher.trigger(createSceneEvent);

	LOG_INFO << "Created new scene with ID: " << tls.sceneRegistry.get<SceneInfoPBComp>(scene).guid();
	return scene;
}

// Destroy a scene
void SceneUtil::DestroyScene(const DestroySceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for destroying scene";
		return;
	}

	auto* pServerComp = tls.gameNodeRegistry.try_get<NodeSceneComp>(param.node);
	if (!pServerComp) {
		LOG_ERROR << "ServerComp not found for node";
		return;
	}

	auto* sceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComp>(param.scene);
	if (!sceneInfo) {
		LOG_ERROR << "SceneInfo not found for scene";
		return;
	}

	OnDestroyScene destroySceneEvent;
	destroySceneEvent.set_entity(entt::to_integral(param.scene));
	tls.dispatcher.trigger(destroySceneEvent);

	pServerComp->RemoveScene(param.scene);

	LOG_INFO << "Destroyed scene with ID: " << sceneInfo->guid();
}

// Handle server node destruction
void SceneUtil::HandleDestroyGameNode(entt::entity node) {
	auto& nodeSceneComp = tls.gameNodeRegistry.get<NodeSceneComp>(node);
	auto sceneLists = nodeSceneComp.GetSceneLists();

	// Destroy all scenes associated with the server node
	for (auto& confIdSceneList : sceneLists | std::views::values) {
		for (auto scene : confIdSceneList) {
			DestroyScene({ node, scene });
		}
	}

	// Destroy the server node itself
	Destroy(tls.gameNodeRegistry, node);

	// Log server destruction
	LOG_INFO << "Destroyed server with ID: " << entt::to_integral(node);
}


// Check if a player can enter a scene
uint32_t SceneUtil::CheckPlayerEnterScene(const EnterSceneParam& param) {
	if (!tls.sceneRegistry.valid(param.scene)) {
		LOG_ERROR << "Invalid scene entity when checking player enter scene - Scene ID: " << entt::to_integral(param.scene);
		return kInvalidEnterSceneParameters;
	}

	auto* sceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComp>(param.scene);
	if (!sceneInfo) {
		LOG_ERROR << "SceneInfo not found when checking player enter scene - Scene ID: " << entt::to_integral(param.scene);
		return kInvalidEnterSceneParameters;
	}

	auto creatorId = tls.registry.get<Guid>(param.enter);
	if (sceneInfo->creators().find(creatorId) == sceneInfo->creators().end()) {
		LOG_WARN << "Player cannot enter scene due to creator restriction - Scene ID: " << entt::to_integral(param.scene);
		return kCheckEnterSceneCreator;
	}

	return kOK;
}


// Check if scene player size limits are respected
uint32_t SceneUtil::CheckScenePlayerSize(entt::entity scene) {
	auto& scenePlayers = tls.sceneRegistry.get<ScenePlayers>(scene);

	if (scenePlayers.size() >= kMaxScenePlayer) {
		LOG_WARN << "Scene player size limit exceeded - Scene ID: " << entt::to_integral(scene);
		return kEnterSceneNotFull;
	}

	auto* gsPlayerInfo = tls.sceneRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(scene);
	if (!gsPlayerInfo) {
		LOG_ERROR << "GameNodePlayerInfoPtr not found for scene - Scene ID: " << entt::to_integral(scene);
		return kEnterSceneGsInfoNull;
	}

	if ((*gsPlayerInfo)->player_size() >= kMaxServerPlayerSize) {
		LOG_WARN << "Game node player size limit exceeded - Scene ID: " << entt::to_integral(scene);
		return kEnterSceneGsFull;
	}

	return kOK;
}



// Enter a player into a scene
void SceneUtil::EnterScene(const EnterSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering scene";
		return;
	}

	auto& scenePlayers = tls.sceneRegistry.get<ScenePlayers>(param.scene);
	scenePlayers.emplace(param.enter);
	tls.registry.emplace<SceneEntityComp>(param.enter, param.scene);

	auto* gsPlayerInfo = tls.sceneRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(param.scene);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() + 1);
	}

	AfterEnterScene afterEnterScene;
	afterEnterScene.set_entity(entt::to_integral(param.enter));
	tls.dispatcher.trigger(afterEnterScene);

	if (tls.registry.any_of<Guid>(param.enter)) {
		LOG_INFO << "Player entered scene - Player GUID: " << tls.registry.get<Guid>(param.enter) << ", Scene ID: " << entt::to_integral(param.scene);
	}
}


// Enter a player into the default scene
void SceneUtil::EnterDefaultScene(const EnterDefaultSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering default scene";
		return;
	}

	// Get a scene that is not full from the NodeSceneSystem
	auto defaultScene = NodeSceneUtil::FindNotFullScene({});

	// Enter the player into the retrieved default scene
	EnterScene({ defaultScene, param.enter });

	// Log the entry into the default scene
	if (tls.registry.any_of<Guid>(param.enter))
	{
		LOG_INFO << "Player entered default scene - Player GUID: " << tls.registry.get<Guid>(param.enter) << ", Scene ID: " << entt::to_integral(defaultScene);
	}	
}


// Remove a player from a scene
void SceneUtil::LeaveScene(const LeaveSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when leaving scene";
		return;
	}

	auto sceneEntity = tls.registry.get<SceneEntityComp>(param.leaver).sceneEntity;
	if (!tls.sceneRegistry.valid(sceneEntity)) {
		LOG_ERROR << "Invalid scene entity when leaving scene - Player GUID: " << tls.registry.get<Guid>(param.leaver);
		return;
	}

    BeforeLeaveScene beforeLeaveScene;
    beforeLeaveScene.set_entity(entt::to_integral(param.leaver));
    tls.dispatcher.trigger(beforeLeaveScene);

	auto& scenePlayers = tls.sceneRegistry.get<ScenePlayers>(sceneEntity);
	scenePlayers.erase(param.leaver);
	tls.registry.remove<SceneEntityComp>(param.leaver);

	auto* gsPlayerInfo = tls.sceneRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(sceneEntity);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() - 1);
	}

	/*AfterLeaveScene afterLeaveScene;
	afterLeaveScene.set_entity(entt::to_integral(param.leaver));
	tls.dispatcher.trigger(afterLeaveScene);*/

	if (tls.registry.any_of<Guid>(param.leaver)) {
		LOG_INFO << "Player left scene - Player GUID: " << tls.registry.get<Guid>(param.leaver) << ", Scene ID: " << entt::to_integral(sceneEntity);
	}
}

// Force a player to change scenes
void SceneUtil::CompelPlayerChangeScene(const CompelChangeSceneParam& param) {
	auto& destNodeScene = tls.gameNodeRegistry.get<NodeSceneComp>(param.destNode);
	auto sceneEntity = destNodeScene.GetSceneWithMinPlayerCountByConfigId(param.sceneConfId);

	if (sceneEntity == entt::null) {
		CreateGameNodeSceneParam p{ .node = param.destNode };
		p.sceneInfo.set_scene_confid(param.sceneConfId);
		sceneEntity = CreateScene2GameNode(p);
	}

	LeaveScene({ param.player });
	if (sceneEntity == entt::null) {
		EnterDefaultScene({ param.player });
		return;
	}

	EnterScene({ sceneEntity, param.player });
}

// Replace a crashed server node with a new node
void SceneUtil::ReplaceCrashGameNode(entt::entity crashNode, entt::entity destNode) {
	auto& crashNodeScene = tls.gameNodeRegistry.get<NodeSceneComp>(crashNode);
	auto sceneLists = crashNodeScene.GetSceneLists();

	for (auto& confIdSceneList : sceneLists | std::views::values) {
		for (auto scene : confIdSceneList) {
			auto* pSceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComp>(scene);
			if (!pSceneInfo) {
				continue;
			}
			CreateGameNodeSceneParam p{ .node = destNode };
			p.sceneInfo.set_scene_confid(pSceneInfo->scene_confid());
			CreateScene2GameNode(p);
		}
	}

	Destroy(tls.gameNodeRegistry, crashNode);
	LOG_INFO << "Replaced crashed server with new node: " << entt::to_integral(destNode);
}
