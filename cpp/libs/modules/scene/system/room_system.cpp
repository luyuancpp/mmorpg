#include "room_system.h"
#include "muduo/base/Logging.h"

#include "modules/scene/comp/scene_comp.h"
#include "engine/core/node/constants/node_constants.h"

#include "table/proto/tip/scene_error_tip.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "proto/logic/component/game_node_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"
#include "proto/common/node.pb.h"
#include "engine/threading/node_context_manager.h"
#include "engine/threading/dispatcher_manager.h"

#include <ranges> // Only if using C++20 ranges
#include <threading/registry_manager.h>
#include <scene/comp/node_scene_comp.h>

thread_local TransientNode12BitCompositeIdGenerator  nodeSequence; // Sequence for generating node IDs

// Constants
static constexpr std::size_t kMaxScenePlayer = 1000;

// Type alias
using GameNodePlayerInfoPtrPBComponent = std::shared_ptr<GameNodePlayerInfoPBComponent>;

// Static function
void SetServerSequenceNodeId(uint32_t nodeId) {
	LOG_TRACE << "Setting server sequence node ID to: " << nodeId;
	RoomUtil::SetSequenceNodeId(nodeId);
}

// Function to add main scene node components
void AddMainRoomToNodeComponent(entt::registry& reg, const entt::entity node) {
	LOG_TRACE << "Adding main scene node components for entity: " << entt::to_integral(node);
	reg.emplace<MainSceneNode>(node);
	reg.emplace<NodeSceneComp>(node);
	reg.emplace<GameNodePlayerInfoPtrPBComponent>(node, std::make_shared<GameNodePlayerInfoPBComponent>());
}

// ScenesSystem implementation
RoomUtil::RoomUtil() {
	LOG_TRACE << "ScenesSystem constructor called";
	Clear();
}

RoomUtil::~RoomUtil() {
	LOG_TRACE << "ScenesSystem destructor called";
	Clear();
}

NodeId RoomUtil::GetGameNodeIdFromGuid(uint64_t scene_id)
{
	return nodeSequence.node_id(static_cast<NodeId>(scene_id));
}

entt::entity RoomUtil::get_game_node_eid(uint64_t scene_id)
{
	return entt::entity{ nodeSequence.node_id(static_cast<NodeId>(scene_id)) };
}

void RoomUtil::SetSequenceNodeId(const uint32_t node_id) { nodeSequence.set_node_id(node_id); }

void RoomUtil::Clear() {
	LOG_TRACE << "Clearing scene system data";
	tlsRegistryManager.roomRegistry.clear();
	tlsRegistryManager.actorRegistry.clear();
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
}

// Get game node ID associated with a scene entity
NodeId RoomUtil::GetGameNodeIdFromRoomEntity(entt::entity scene) {
	auto* sceneInfo = tlsRegistryManager.roomRegistry.try_get<SceneInfoPBComponent>(scene);
	if (sceneInfo) {
		return GetGameNodeIdFromGuid(sceneInfo->guid());
	}
	else {
		LOG_ERROR << "SceneInfo not found for entity: " << entt::to_integral(scene);
		return kInvalidNodeId;
	}
}

// Generate unique scene ID
uint32_t RoomUtil::GenRoomGuid() {
	uint32_t sceneId = nodeSequence.Generate();
	while (tlsRegistryManager.roomRegistry.valid(entt::entity{ sceneId })) {
		sceneId = nodeSequence.Generate();
	}
	LOG_INFO << "Generated new scene ID: " << sceneId;
	return sceneId;
}

// Get total number of scenes associated with a specific configuration ID
std::size_t RoomUtil::GetRoomsSize(uint32_t sceneConfigId) {
	std::size_t sceneSize = 0;
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	for (auto node : registry.view<NodeSceneComp>()) {
		auto& nodeSceneComp = registry.get<NodeSceneComp>(node);
		sceneSize += nodeSceneComp.GetScenesByConfig(sceneConfigId).size();
	}
	LOG_TRACE << "Total scenes size for config ID " << sceneConfigId << ": " << sceneSize;
	return sceneSize;
}

// Get total number of scenes in the registry
std::size_t RoomUtil::GetRoomsSize() {
	std::size_t totalScenes = tlsRegistryManager.roomRegistry.storage<SceneInfoPBComponent>().size();
	LOG_TRACE << "Total scenes in the registry: " << totalScenes;
	return totalScenes;
}

// Check if scene registry is empty
bool RoomUtil::IsRoomEmpty() {
	bool isEmpty = tlsRegistryManager.roomRegistry.storage<SceneInfoPBComponent>().empty();
	LOG_TRACE << "Scene registry empty: " << (isEmpty ? "true" : "false");
	return isEmpty;
}

// Check if there are non-empty scene lists for a specific configuration
bool RoomUtil::ConfigRoomListNotEmpty(uint32_t sceneConfigId) {
	auto& sceneNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	for (auto nodeEid : sceneNodeRegistry.view<NodeSceneComp>()) {
		auto& nodeSceneComp = sceneNodeRegistry.get<NodeSceneComp>(nodeEid);
		if (!nodeSceneComp.GetScenesByConfig(sceneConfigId).empty()) {
			LOG_TRACE << "Non-empty scene list found for config ID: " << sceneConfigId;
			return true;
		}
	}
	LOG_TRACE << "No non-empty scene list found for config ID: " << sceneConfigId;
	return false;
}
//
//✅ 2. 多节点负载均衡
//你在 CompelPlayerChangeScene 中如果找不到场景就直接创建了：
//
//cpp
//复制
//编辑
//sceneEntity = CreateSceneToSceneNode(p);
//更好的做法是考虑负载均衡，比如：
//
//查找资源空闲的节点（CPU / 玩家少）。
//
//优先选择已有场景，再决定是否创建。

// Create a new scene associated with a game node
entt::entity RoomUtil::CreateRoomOnRoomNode(const CreateSceneNodeRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for creating scene";
		return entt::null;
	}

	SceneInfoPBComponent sceneInfo(param.roomInfo);
	if (sceneInfo.guid() <= 0) {
		sceneInfo.set_guid(GenRoomGuid());
	}

	const auto scene = TryCreateEntity(tlsRegistryManager.roomRegistry, entt::entity{ sceneInfo.guid() });
	if (scene == entt::null) {
		LOG_ERROR << "Failed to create scene in registry";
		return entt::null;
	}

	tlsRegistryManager.roomRegistry.emplace<SceneInfoPBComponent>(scene, std::move(sceneInfo));
	tlsRegistryManager.roomRegistry.emplace<ScenePlayers>(scene);

	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto* serverPlayerInfo = registry.try_get<GameNodePlayerInfoPtrPBComponent>(param.node);
	if (serverPlayerInfo) {
		tlsRegistryManager.roomRegistry.emplace<GameNodePlayerInfoPtrPBComponent>(scene, *serverPlayerInfo);
	}

	auto* pServerComp = registry.try_get<NodeSceneComp>(param.node);
	if (pServerComp) {
		pServerComp->AddScene(scene);
	}

	OnSceneCreate createSceneEvent;
	createSceneEvent.set_entity(entt::to_integral(scene));
	dispatcher.trigger(createSceneEvent);

	LOG_INFO << "Created new scene with ID: " << tlsRegistryManager.roomRegistry.get<SceneInfoPBComponent>(scene).guid();
	return scene;
}

// Destroy a scene
void RoomUtil::DestroyRoom(const DestroyRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for destroying scene";
		return;
	}

	auto* pServerComp = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<NodeSceneComp>(param.node);
	if (!pServerComp) {
		LOG_ERROR << "ServerComp not found for node";
		return;
	}

	auto* sceneInfo = tlsRegistryManager.roomRegistry.try_get<SceneInfoPBComponent>(param.room);
	if (!sceneInfo) {
		LOG_ERROR << "SceneInfo not found for scene";
		return;
	}

	OnDestroyScene destroySceneEvent;
	destroySceneEvent.set_entity(entt::to_integral(param.room));
	dispatcher.trigger(destroySceneEvent);

	pServerComp->RemoveScene(param.room);

	LOG_INFO << "Destroyed scene with ID: " << sceneInfo->guid();
}

// Handle server node destruction
void RoomUtil::HandleDestroyRoomNode(entt::entity node) {
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	auto& nodeSceneComp = registry.get<NodeSceneComp>(node);
	auto sceneLists = nodeSceneComp.GetSceneLists();

	// Destroy all scenes associated with the server node
	for (auto& confIdSceneList : sceneLists | std::views::values) {
		for (auto scene : confIdSceneList) {
			DestroyRoom({ node, scene });
		}
	}

	// Destroy the server node itself
	Destroy(registry, node);

	// Log server destruction
	LOG_INFO << "Destroyed server with ID: " << entt::to_integral(node);
}


// Check if a player can enter a scene
uint32_t RoomUtil::CheckPlayerEnterRoom(const EnterRoomParam& param) {
	if (!tlsRegistryManager.roomRegistry.valid(param.room)) {
		LOG_ERROR << "Invalid scene entity when checking player enter scene - Scene ID: " << entt::to_integral(param.room);
		return kInvalidEnterSceneParameters;
	}

	auto* sceneInfo = tlsRegistryManager.roomRegistry.try_get<SceneInfoPBComponent>(param.room);
	if (!sceneInfo) {
		LOG_ERROR << "SceneInfo not found when checking player enter scene - Scene ID: " << entt::to_integral(param.room);
		return kInvalidEnterSceneParameters;
	}

	auto creatorId = tlsRegistryManager.actorRegistry.get<Guid>(param.enter);
	if (sceneInfo->creators().find(creatorId) == sceneInfo->creators().end()) {
		LOG_WARN << "Player cannot enter scene due to creator restriction - Scene ID: " << entt::to_integral(param.room);
		return kCheckEnterSceneCreator;
	}

	return kSuccess;
}


// Check if scene player size limits are respected
uint32_t RoomUtil::HasRoomSlot(entt::entity scene) {
	auto& scenePlayers = tlsRegistryManager.roomRegistry.get<ScenePlayers>(scene);

	if (scenePlayers.size() >= kMaxScenePlayer) {
		LOG_WARN << "Scene player size limit exceeded - Scene ID: " << entt::to_integral(scene);
		return kEnterSceneNotFull;
	}

	auto* gsPlayerInfo = tlsRegistryManager.roomRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(scene);
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


// Enter a player into a scene
void RoomUtil::EnterRoom(const EnterRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering scene";
		return;
	}

	if (!tlsRegistryManager.roomRegistry.valid(param.room))
	{
		LOG_ERROR << "Invalid scene entity when entering scene - Scene ID: " << entt::to_integral(param.room);
		return;
	}

	if (!tlsRegistryManager.actorRegistry.valid(param.enter))
	{
		LOG_ERROR << "Invalid player entity when entering scene - Player : " << entt::to_integral(param.enter);
		return;
	}

	auto& scenePlayers = tlsRegistryManager.roomRegistry.get<ScenePlayers>(param.room);
	scenePlayers.emplace(param.enter);
	if (tlsRegistryManager.actorRegistry.any_of<SceneEntityComp>(param.enter))
	{
		LOG_FATAL << tlsRegistryManager.actorRegistry.get<Guid>(param.enter);
	}
	tlsRegistryManager.actorRegistry.emplace<SceneEntityComp>(param.enter, param.room);

	auto* gsPlayerInfo = tlsRegistryManager.roomRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(param.room);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() + 1);
	}

	AfterEnterScene afterEnterScene;
	afterEnterScene.set_entity(entt::to_integral(param.enter));
	dispatcher.trigger(afterEnterScene);

	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.enter)) {
		LOG_INFO << "Player entered scene - Player GUID: " << tlsRegistryManager.actorRegistry.get<Guid>(param.enter) << ", Scene ID: " << entt::to_integral(param.room);
	}
}


// Enter a player into the default scene
void RoomUtil::EnterDefaultRoom(const EnterDefaultRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering default scene";
		return;
	}

	// Get a scene that is not full from the NodeSceneSystem
	auto defaultScene = NodeSceneSystem::FindNotFullScene({});

	// Enter the player into the retrieved default scene
	EnterRoom({ defaultScene, param.enter });

	// Log the entry into the default scene
	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.enter))
	{
		LOG_INFO << "Player entered default scene - Player GUID: " << tlsRegistryManager.actorRegistry.get<Guid>(param.enter) << ", Scene ID: " << entt::to_integral(defaultScene);
	}	
}


// Remove a player from a scene
void RoomUtil::LeaveRoom(const LeaveRoomParam& param) {
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
	if (!tlsRegistryManager.roomRegistry.valid(sceneEntity)) {
		LOG_ERROR << "Invalid scene entity when leaving scene - Player : " << entt::to_integral(param.leaver);
		return;
	}

    BeforeLeaveScene beforeLeaveScene;
    beforeLeaveScene.set_entity(entt::to_integral(param.leaver));
    dispatcher.trigger(beforeLeaveScene);

	auto& scenePlayers = tlsRegistryManager.roomRegistry.get<ScenePlayers>(sceneEntity);
	scenePlayers.erase(param.leaver);
	tlsRegistryManager.actorRegistry.remove<SceneEntityComp>(param.leaver);

	auto* gsPlayerInfo = tlsRegistryManager.roomRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(sceneEntity);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() - 1);
	}

	/*AfterLeaveScene afterLeaveScene;
	afterLeaveScene.set_entity(entt::to_integral(param.leaver));
	dispatcher.trigger(afterLeaveScene);*/

	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.leaver)) {
		LOG_INFO << "Player left scene - Player GUID: " << tlsRegistryManager.actorRegistry.get<Guid>(param.leaver) << ", Scene ID: " << entt::to_integral(sceneEntity);
	}
}

// 这里只处理了同gs,如果是跨gs的场景切换，应该别的地方处理
void RoomUtil::CompelPlayerChangeRoom(const CompelChangeRoomParam& param) {
	auto& destNodeScene = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<NodeSceneComp>(param.destNode);
	auto sceneEntity = destNodeScene.GetSceneWithMinPlayerCountByConfigId(param.sceneConfId);

	if (sceneEntity == entt::null) {
		CreateSceneNodeRoomParam p{ .node = param.destNode };
		p.roomInfo.set_scene_confid(param.sceneConfId);
		sceneEntity = CreateRoomOnRoomNode(p);
	}

	LeaveRoom({ param.player });
	if (sceneEntity == entt::null) {
		EnterDefaultRoom({ param.player });
		return;
	}

	EnterRoom({ sceneEntity, param.player });
}

// Replace a crashed server node with a new node
void RoomUtil::ReplaceCrashRoomNode(entt::entity crashNode, entt::entity destNode) {
	auto& sceneRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto& crashNodeScene = sceneRegistry.get<NodeSceneComp>(crashNode);
	auto sceneLists = crashNodeScene.GetSceneLists();

	for (auto& confIdSceneList : sceneLists | std::views::values) {
		for (auto scene : confIdSceneList) {
			auto* pSceneInfo = tlsRegistryManager.roomRegistry.try_get<SceneInfoPBComponent>(scene);
			if (!pSceneInfo) {
				continue;
			}
			CreateSceneNodeRoomParam p{ .node = destNode };
			p.roomInfo.set_scene_confid(pSceneInfo->scene_confid());
			CreateRoomOnRoomNode(p);
		}
	}

	Destroy(sceneRegistry, crashNode);
	LOG_INFO << "Replaced crashed server with new node: " << entt::to_integral(destNode);
}
