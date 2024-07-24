#include "system/scene/scene_system.h"
#include "muduo/base/Logging.h"

#include "comp/scene.h"
#include "constants/node.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"

#include "proto/logic/component/gs_node_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"

#include <ranges> // Only if using C++20 ranges

// Constants
static constexpr std::size_t kMaxScenePlayer = 1000;

// Type alias
using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;

// Static function
void SetServerSequenceNodeId(uint32_t nodeId) {
	LOG_TRACE << "Setting server sequence node ID to: " << nodeId;
	ScenesSystem::SetSequenceNodeId(nodeId);
}

// Function to add main scene node components
void AddMainSceneNodeComponent(entt::registry& reg, const entt::entity node) {
	LOG_TRACE << "Adding main scene node components for entity: " << entt::to_integral(node);
	reg.emplace<MainSceneServer>(node);
	reg.emplace<ServerComp>(node);
	reg.emplace<GameNodePlayerInfoPtr>(node, std::make_shared<GameNodeInfo>());
}

// ScenesSystem implementation
ScenesSystem::ScenesSystem() {
	LOG_TRACE << "ScenesSystem constructor called";
	Clear();
}

ScenesSystem::~ScenesSystem() {
	LOG_TRACE << "ScenesSystem destructor called";
	Clear();
}

void ScenesSystem::Clear() {
	LOG_TRACE << "Clearing scene system data";
	tls.scene_registry.clear();
	tls.registry.clear();
	tls.game_node_registry.clear();
}

// Get game node ID associated with a scene entity
NodeId ScenesSystem::GetGameNodeId(entt::entity scene) {
	auto* sceneInfo = tls.scene_registry.try_get<SceneInfo>(scene);
	if (sceneInfo) {
		return GetGameNodeId(sceneInfo->guid());
	}
	else {
		LOG_ERROR << "SceneInfo not found for entity: " << entt::to_integral(scene);
		return kInvalidNodeId;
	}
}

// Generate unique scene ID
uint32_t ScenesSystem::GenSceneGuid() {
	uint32_t sceneId = nodeSequence.Generate();
	while (tls.scene_registry.valid(entt::entity{ sceneId })) {
		sceneId = nodeSequence.Generate();
	}
	LOG_DEBUG << "Generated new scene ID: " << sceneId;
	return sceneId;
}

// Get total number of scenes associated with a specific configuration ID
std::size_t ScenesSystem::GetScenesSize(uint32_t sceneConfigId) {
	std::size_t sceneSize = 0;
	for (auto node : tls.game_node_registry.view<ServerComp>()) {
		auto& serverComp = tls.game_node_registry.get<ServerComp>(node);
		sceneSize += serverComp.GetSceneListByConfig(sceneConfigId).size();
	}
	LOG_TRACE << "Total scenes size for config ID " << sceneConfigId << ": " << sceneSize;
	return sceneSize;
}

// Get total number of scenes in the registry
std::size_t ScenesSystem::GetScenesSize() {
	std::size_t totalScenes = tls.scene_registry.storage<SceneInfo>().size();
	LOG_TRACE << "Total scenes in the registry: " << totalScenes;
	return totalScenes;
}

// Check if scene registry is empty
bool ScenesSystem::IsSceneEmpty() {
	bool isEmpty = tls.scene_registry.storage<SceneInfo>().empty();
	LOG_TRACE << "Scene registry empty: " << (isEmpty ? "true" : "false");
	return isEmpty;
}

// Check if there are non-empty scene lists for a specific configuration
bool ScenesSystem::ConfigSceneListNotEmpty(uint32_t sceneConfigId) {
	for (auto nodeEid : tls.game_node_registry.view<ServerComp>()) {
		auto& serverComp = tls.game_node_registry.get<ServerComp>(nodeEid);
		if (!serverComp.GetSceneListByConfig(sceneConfigId).empty()) {
			LOG_TRACE << "Non-empty scene list found for config ID: " << sceneConfigId;
			return true;
		}
	}
	LOG_TRACE << "No non-empty scene list found for config ID: " << sceneConfigId;
	return false;
}

// Create a new scene associated with a game node
entt::entity ScenesSystem::CreateScene2GameNode(const CreateGameNodeSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for creating scene";
		return entt::null;
	}

	SceneInfo sceneInfo(param.sceneInfo);
	if (sceneInfo.guid() <= 0) {
		sceneInfo.set_guid(GenSceneGuid());
	}

	const auto scene = tls.scene_registry.create(entt::entity{ sceneInfo.guid() });
	if (scene == entt::null) {
		LOG_ERROR << "Failed to create scene in registry";
		return entt::null;
	}

	tls.scene_registry.emplace<SceneInfo>(scene, std::move(sceneInfo));
	tls.scene_registry.emplace<ScenePlayers>(scene);

	auto* serverPlayerInfo = tls.game_node_registry.try_get<GameNodePlayerInfoPtr>(param.node);
	if (serverPlayerInfo) {
		tls.scene_registry.emplace<GameNodePlayerInfoPtr>(scene, *serverPlayerInfo);
	}

	auto* pServerComp = tls.game_node_registry.try_get<ServerComp>(param.node);
	if (pServerComp) {
		pServerComp->AddScene(scene);
	}

	OnSceneCreate createSceneEvent;
	createSceneEvent.set_entity(entt::to_integral(scene));
	tls.dispatcher.trigger(createSceneEvent);

	LOG_DEBUG << "Created new scene with ID: " << tls.scene_registry.get<SceneInfo>(scene).guid();
	return scene;
}

// Destroy a scene
void ScenesSystem::DestroyScene(const DestroySceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for destroying scene";
		return;
	}

	auto* pServerComp = tls.game_node_registry.try_get<ServerComp>(param.node);
	if (!pServerComp) {
		LOG_ERROR << "ServerComp not found for node";
		return;
	}

	auto* sceneInfo = tls.scene_registry.try_get<SceneInfo>(param.scene);
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
void ScenesSystem::OnDestroyServer(entt::entity node) {
	auto& serverComp = tls.game_node_registry.get<ServerComp>(node);
	auto sceneLists = serverComp.GetSceneList();

	// Destroy all scenes associated with the server node
	for (auto& confIdSceneList : sceneLists | std::views::values) {
		for (auto scene : confIdSceneList) {
			DestroyScene({ node, scene });
		}
	}

	// Destroy the server node itself
	Destroy(tls.game_node_registry, node);

	// Log server destruction
	LOG_INFO << "Destroyed server with ID: " << entt::to_integral(node);
}


// Check if a player can enter a scene
uint32_t ScenesSystem::CheckPlayerEnterScene(const EnterSceneParam& param) {
	if (!tls.scene_registry.valid(param.scene)) {
		LOG_ERROR << "Invalid scene entity when checking player enter scene - Scene ID: " << entt::to_integral(param.scene);
		return kRetCheckEnterSceneSceneParam;
	}

	auto* sceneInfo = tls.scene_registry.try_get<SceneInfo>(param.scene);
	if (!sceneInfo) {
		LOG_ERROR << "SceneInfo not found when checking player enter scene - Scene ID: " << entt::to_integral(param.scene);
		return kRetCheckEnterSceneSceneParam;
	}

	auto creatorId = tls.registry.get<Guid>(param.enter);
	if (sceneInfo->creators().find(creatorId) == sceneInfo->creators().end()) {
		LOG_WARN << "Player cannot enter scene due to creator restriction - Scene ID: " << entt::to_integral(param.scene);
		return kRetCheckEnterSceneCreator;
	}

	return kOK;
}


// Check if scene player size limits are respected
uint32_t ScenesSystem::CheckScenePlayerSize(entt::entity scene) {
	auto& scenePlayers = tls.scene_registry.get<ScenePlayers>(scene);

	if (scenePlayers.size() >= kMaxScenePlayer) {
		LOG_WARN << "Scene player size limit exceeded - Scene ID: " << entt::to_integral(scene);
		return kRetEnterSceneNotFull;
	}

	auto* gsPlayerInfo = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(scene);
	if (!gsPlayerInfo) {
		LOG_ERROR << "GameNodePlayerInfoPtr not found for scene - Scene ID: " << entt::to_integral(scene);
		return kRetEnterSceneGsInfoNull;
	}

	if ((*gsPlayerInfo)->player_size() >= kMaxServerPlayerSize) {
		LOG_WARN << "Game node player size limit exceeded - Scene ID: " << entt::to_integral(scene);
		return kRetEnterSceneGsFull;
	}

	return kOK;
}



// Enter a player into a scene
void ScenesSystem::EnterScene(const EnterSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering scene";
		return;
	}

	auto& scenePlayers = tls.scene_registry.get<ScenePlayers>(param.scene);
	scenePlayers.emplace(param.enter);
	tls.registry.emplace<SceneEntity>(param.enter, param.scene);

	auto* gsPlayerInfo = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(param.scene);
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
void ScenesSystem::EnterDefaultScene(const EnterDefaultSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering default scene";
		return;
	}

	// Get a scene that is not full from the NodeSceneSystem
	auto defaultScene = NodeSceneSystem::GetNotFullScene({});

	// Enter the player into the retrieved default scene
	EnterScene({ defaultScene, param.enter });

	// Log the entry into the default scene
	if (tls.registry.any_of<Guid>(param.enter))
	{
		LOG_INFO << "Player entered default scene - Player GUID: " << tls.registry.get<Guid>(param.enter) << ", Scene ID: " << entt::to_integral(defaultScene);
	}	
}


// Remove a player from a scene
void ScenesSystem::LeaveScene(const LeaveSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when leaving scene";
		return;
	}

	auto sceneEntity = tls.registry.get<SceneEntity>(param.leaver).sceneEntity;
	if (!tls.scene_registry.valid(sceneEntity)) {
		LOG_ERROR << "Invalid scene entity when leaving scene - Player GUID: " << tls.registry.get<Guid>(param.leaver);
		return;
	}

	auto& scenePlayers = tls.scene_registry.get<ScenePlayers>(sceneEntity);
	scenePlayers.erase(param.leaver);
	tls.registry.remove<SceneEntity>(param.leaver);

	auto* gsPlayerInfo = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(sceneEntity);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() - 1);
	}

	AfterLeaveScene afterLeaveScene;
	afterLeaveScene.set_entity(entt::to_integral(param.leaver));
	tls.dispatcher.trigger(afterLeaveScene);

	if (tls.registry.any_of<Guid>(param.leaver)) {
		LOG_INFO << "Player left scene - Player GUID: " << tls.registry.get<Guid>(param.leaver) << ", Scene ID: " << entt::to_integral(sceneEntity);
	}
}

// Force a player to change scenes
void ScenesSystem::CompelPlayerChangeScene(const CompelChangeSceneParam& param) {
	auto& destNodeScene = tls.game_node_registry.get<ServerComp>(param.destNode);
	auto sceneEntity = destNodeScene.GetMinPlayerSizeSceneByConfigId(param.sceneConfId);

	if (sceneEntity == entt::null) {
		CreateGameNodeSceneParam p{ .node = param.destNode };
		p.sceneInfo.set_scene_confid(param.sceneConfId);
		sceneEntity = CreateScene2GameNode(p);
	}

	LeaveScene({ param.player_ });
	if (sceneEntity == entt::null) {
		EnterDefaultScene({ param.player_ });
		return;
	}

	EnterScene({ sceneEntity, param.player_ });
}

// Replace a crashed server node with a new node
void ScenesSystem::ReplaceCrashServer(entt::entity crashNode, entt::entity destNode) {
	auto& crashNodeScene = tls.game_node_registry.get<ServerComp>(crashNode);
	auto sceneLists = crashNodeScene.GetSceneList();

	for (auto& confIdSceneList : sceneLists | std::views::values) {
		for (auto scene : confIdSceneList) {
			auto* pSceneInfo = tls.scene_registry.try_get<SceneInfo>(scene);
			if (!pSceneInfo) {
				continue;
			}
			CreateGameNodeSceneParam p{ .node = destNode };
			p.sceneInfo.set_scene_confid(pSceneInfo->scene_confid());
			CreateScene2GameNode(p);
		}
	}

	Destroy(tls.game_node_registry, crashNode);
	LOG_INFO << "Replaced crashed server with new node: " << entt::to_integral(destNode);
}
