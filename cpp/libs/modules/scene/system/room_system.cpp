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
static constexpr std::size_t kMaxRoomPlayer = 1000;

// Type alias
using GameNodePlayerInfoPtrPBComponent = std::shared_ptr<GameNodePlayerInfoPBComponent>;

// Static function
void SetServerSequenceNodeId(uint32_t nodeId) {
	LOG_TRACE << "Setting server sequence node ID to: " << nodeId;
	RoomUtil::SetSequenceNodeId(nodeId);
}

void AddMainRoomToNodeComponent(entt::registry& reg, const entt::entity node) {
	LOG_TRACE << "Adding main scene node components for entity: " << entt::to_integral(node);
	reg.emplace<MainRoomNode>(node);
	reg.emplace<NodeNodeComp>(node);
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

NodeId RoomUtil::GetGameNodeIdFromGuid(uint64_t room_id)
{
	return nodeSequence.node_id(static_cast<NodeId>(room_id));
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
NodeId RoomUtil::GetGameNodeIdFromRoomEntity(entt::entity room) {
	auto* roomInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(room);
	if (roomInfo) {
		return GetGameNodeIdFromGuid(roomInfo->guid());
	}
	else {
		LOG_ERROR << "SceneInfo not found for entity: " << entt::to_integral(room);
		return kInvalidNodeId;
	}
}

// Generate unique scene ID
uint32_t RoomUtil::GenRoomGuid() {
	uint32_t roomId = nodeSequence.Generate();
	while (tlsRegistryManager.roomRegistry.valid(entt::entity{ roomId })) {
		roomId = nodeSequence.Generate();
	}
	LOG_INFO << "Generated new scene ID: " << roomId;
	return roomId;
}

// Get total number of scenes associated with a specific configuration ID
std::size_t RoomUtil::GetRoomsSize(uint32_t sceneConfigId) {
	std::size_t roomSize = 0;
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	for (auto node : registry.view<NodeNodeComp>()) {
		auto& nodeRoomComp = registry.get<NodeNodeComp>(node);
		roomSize += nodeRoomComp.GetScenesByConfig(sceneConfigId).size();
	}
	LOG_TRACE << "Total scenes size for config ID " << sceneConfigId << ": " << roomSize;
	return roomSize;
}

// Get total number of scenes in the registry
std::size_t RoomUtil::GetRoomsSize() {
	std::size_t totalRooms = tlsRegistryManager.roomRegistry.storage<RoomInfoPBComponent>().size();
	LOG_TRACE << "Total scenes in the registry: " << totalRooms;
	return totalRooms;
}

// Check if scene registry is empty
bool RoomUtil::IsRoomEmpty() {
	bool isEmpty = tlsRegistryManager.roomRegistry.storage<RoomInfoPBComponent>().empty();
	LOG_TRACE << "Scene registry empty: " << (isEmpty ? "true" : "false");
	return isEmpty;
}

// Check if there are non-empty scene lists for a specific configuration
bool RoomUtil::ConfigRoomListNotEmpty(uint32_t roomConfigId) {
	auto& roomNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	for (auto nodeEid : roomNodeRegistry.view<NodeNodeComp>()) {
		auto& nodeRoomComp = roomNodeRegistry.get<NodeNodeComp>(nodeEid);
		if (!nodeRoomComp.GetScenesByConfig(roomConfigId).empty()) {
			LOG_TRACE << "Non-empty scene list found for config ID: " << roomConfigId;
			return true;
		}
	}
	LOG_TRACE << "No non-empty scene list found for config ID: " << roomConfigId;
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
entt::entity RoomUtil::CreateRoomOnRoomNode(const CreateRoomOnNodeRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for creating scene";
		return entt::null;
	}

	RoomInfoPBComponent roomInfo(param.roomInfo);
	if (roomInfo.guid() <= 0) {
		roomInfo.set_guid(GenRoomGuid());
	}

	const auto room = TryCreateEntity(tlsRegistryManager.roomRegistry, entt::entity{ roomInfo.guid() });
	if (room == entt::null) {
		LOG_ERROR << "Failed to create scene in registry";
		return entt::null;
	}

	tlsRegistryManager.roomRegistry.emplace<RoomInfoPBComponent>(room, std::move(roomInfo));
	tlsRegistryManager.roomRegistry.emplace<RoomPlayers>(room);

	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto* serverPlayerInfo = registry.try_get<GameNodePlayerInfoPtrPBComponent>(param.node);
	if (serverPlayerInfo) {
		tlsRegistryManager.roomRegistry.emplace<GameNodePlayerInfoPtrPBComponent>(room, *serverPlayerInfo);
	}

	auto* pServerComp = registry.try_get<NodeNodeComp>(param.node);
	if (pServerComp) {
		pServerComp->AddScene(room);
	}

	OnRoomCreated createRoomEvent;
	createRoomEvent.set_entity(entt::to_integral(room));
	dispatcher.trigger(createRoomEvent);

	LOG_INFO << "Created new scene with ID: " << tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(room).guid();
	return room;
}

// Destroy a scene
void RoomUtil::DestroyRoom(const DestroyRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for destroying scene";
		return;
	}

	auto* pServerComp = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<NodeNodeComp>(param.node);
	if (!pServerComp) {
		LOG_ERROR << "ServerComp not found for node";
		return;
	}

	auto* roomInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(param.room);
	if (!roomInfo) {
		LOG_ERROR << "SceneInfo not found for scene";
		return;
	}

	OnDestroyScene destroyRoomEvent;
	destroyRoomEvent.set_entity(entt::to_integral(param.room));
	dispatcher.trigger(destroyRoomEvent);

	pServerComp->RemoveScene(param.room);

	LOG_INFO << "Destroyed scene with ID: " << roomInfo->guid();
}

// Handle server node destruction
void RoomUtil::HandleDestroyRoomNode(entt::entity node) {
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	auto& nodeRoomComp = registry.get<NodeNodeComp>(node);
	auto roomList = nodeRoomComp.GetSceneLists();

	// Destroy all scenes associated with the server node
	for (auto& confIdRoomList : roomList | std::views::values) {
		for (auto room : confIdRoomList) {
			DestroyRoom({ node, room });
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

	auto* roomInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(param.room);
	if (!roomInfo) {
		LOG_ERROR << "SceneInfo not found when checking player enter scene - Scene ID: " << entt::to_integral(param.room);
		return kInvalidEnterSceneParameters;
	}

	auto creatorId = tlsRegistryManager.actorRegistry.get<Guid>(param.enter);
	if (roomInfo->creators().find(creatorId) == roomInfo->creators().end()) {
		LOG_WARN << "Player cannot enter scene due to creator restriction - Scene ID: " << entt::to_integral(param.room);
		return kCheckEnterSceneCreator;
	}

	return kSuccess;
}


// Check if scene player size limits are respected
uint32_t RoomUtil::HasRoomSlot(entt::entity room) {
	auto& roomPlayers = tlsRegistryManager.roomRegistry.get<RoomPlayers>(room);

	if (roomPlayers.size() >= kMaxRoomPlayer) {
		LOG_WARN << "Scene player size limit exceeded - Scene ID: " << entt::to_integral(room);
		return kEnterSceneNotFull;
	}

	auto* gsPlayerInfo = tlsRegistryManager.roomRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(room);
	if (!gsPlayerInfo) {
		LOG_ERROR << "GameNodePlayerInfoPtr not found for scene - Scene ID: " << entt::to_integral(room);
		return kEnterSceneGsInfoNull;
	}

	if ((*gsPlayerInfo)->player_size() >= kMaxServerPlayerSize) {
		LOG_WARN << "Game node player size limit exceeded - Scene ID: " << entt::to_integral(room);
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

	auto& roomPlayers = tlsRegistryManager.roomRegistry.get<RoomPlayers>(param.room);
	roomPlayers.emplace(param.enter);
	if (tlsRegistryManager.actorRegistry.any_of<RoomEntityComp>(param.enter))
	{
		LOG_FATAL << tlsRegistryManager.actorRegistry.get<Guid>(param.enter);
	}
	tlsRegistryManager.actorRegistry.emplace<RoomEntityComp>(param.enter, param.room);

	auto* gsPlayerInfo = tlsRegistryManager.roomRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(param.room);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() + 1);
	}

	AfterEnterRoom afterEnterRoom;
	afterEnterRoom.set_entity(entt::to_integral(param.enter));
	dispatcher.trigger(afterEnterRoom);

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
	auto defaultRoom = NodeSceneSystem::FindNotFullRoom({});

	// Enter the player into the retrieved default scene
	EnterRoom({ defaultRoom, param.enter });

	// Log the entry into the default scene
	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.enter))
	{
		LOG_INFO << "Player entered default scene - Player GUID: " << tlsRegistryManager.actorRegistry.get<Guid>(param.enter) << ", Scene ID: " << entt::to_integral(defaultRoom);
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

	auto roomEntityComp = tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(param.leaver);
	if (nullptr == roomEntityComp)
	{
		LOG_ERROR << "SceneEntityComp not found for player when leaving scene - Player : " << entt::to_integral(param.leaver);
		return;
	}

	auto roomEntity = roomEntityComp->roomEntity;
	if (!tlsRegistryManager.roomRegistry.valid(roomEntity)) {
		LOG_ERROR << "Invalid scene entity when leaving scene - Player : " << entt::to_integral(param.leaver);
		return;
	}

    BeforeLeaveRoom beforeLeaveRoom;
    beforeLeaveRoom.set_entity(entt::to_integral(param.leaver));
    dispatcher.trigger(beforeLeaveRoom);

	auto& roomPlayers = tlsRegistryManager.roomRegistry.get<RoomPlayers>(roomEntity);
	roomPlayers.erase(param.leaver);
	tlsRegistryManager.actorRegistry.remove<RoomEntityComp>(param.leaver);

	auto* gsPlayerInfo = tlsRegistryManager.roomRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(roomEntity);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() - 1);
	}

	/*AfterLeaveRoom afterLeaveRoom;
	afterLeaveRoom.set_entity(entt::to_integral(param.leaver));
	dispatcher.trigger(afterLeaveRoom);*/

	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.leaver)) {
		LOG_INFO << "Player left scene - Player GUID: " << tlsRegistryManager.actorRegistry.get<Guid>(param.leaver) << ", Scene ID: " << entt::to_integral(roomEntity);
	}
}

// 这里只处理了同gs,如果是跨gs的场景切换，应该别的地方处理
void RoomUtil::CompelPlayerChangeRoom(const CompelChangeRoomParam& param) {
	auto& destNodeRoom = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).get<NodeNodeComp>(param.destNode);
	auto roomEntity = destNodeRoom.GetSceneWithMinPlayerCountByConfigId(param.sceneConfId);

	if (roomEntity == entt::null) {
		CreateRoomOnNodeRoomParam p{ .node = param.destNode };
		p.roomInfo.set_scene_confid(param.sceneConfId);
		roomEntity = CreateRoomOnRoomNode(p);
	}

	LeaveRoom({ param.player });
	if (roomEntity == entt::null) {
		EnterDefaultRoom({ param.player });
		return;
	}

	EnterRoom({ roomEntity, param.player });
}

// Replace a crashed server node with a new node
void RoomUtil::ReplaceCrashRoomNode(entt::entity crashNode, entt::entity destNode) {
	auto& roomRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto& crashNodeRoom = roomRegistry.get<NodeNodeComp>(crashNode);
	auto roomList = crashNodeRoom.GetSceneLists();

	for (auto& confIdRoomList : roomList | std::views::values) {
		for (auto room : confIdRoomList) {
			auto* roomInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(room);
			if (!roomInfo) {
				continue;
			}
			CreateRoomOnNodeRoomParam p{ .node = destNode };
			p.roomInfo.set_scene_confid(roomInfo->scene_confid());
			CreateRoomOnRoomNode(p);
		}
	}

	Destroy(roomRegistry, crashNode);
	LOG_INFO << "Replaced crashed server with new node: " << entt::to_integral(destNode);
}
