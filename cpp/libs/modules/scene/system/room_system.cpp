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
	LOG_TRACE << "Adding main room node components for entity: " << entt::to_integral(node);
	reg.emplace<MainRoomNode>(node);
	reg.emplace<NodeNodeComp>(node);
	reg.emplace<GameNodePlayerInfoPtrPBComponent>(node, std::make_shared<GameNodePlayerInfoPBComponent>());
}

// RoomUtil implementation
RoomUtil::RoomUtil() {
	LOG_TRACE << "RoomSystem constructor called";
	ClearAllRoomData();
}

RoomUtil::~RoomUtil() {
	LOG_TRACE << "RoomSystem destructor called";
	ClearAllRoomData();
}

NodeId RoomUtil::GetGameNodeIdFromGuid(uint64_t room_id)
{
	return nodeSequence.node_id(static_cast<NodeId>(room_id));
}

entt::entity RoomUtil::GetRoomNodeEntityId(uint64_t room_id)
{
	return entt::entity{ nodeSequence.node_id(static_cast<NodeId>(room_id)) };
}

void RoomUtil::SetSequenceNodeId(const uint32_t node_id) { nodeSequence.set_node_id(node_id); }

void RoomUtil::ClearAllRoomData() {
	LOG_TRACE << "Clearing room system data";
	tlsRegistryManager.roomRegistry.clear();
	tlsRegistryManager.actorRegistry.clear();
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
}

// Get game node ID associated with a room entity
NodeId RoomUtil::GetGameNodeIdFromRoomEntity(entt::entity room) {
	auto* roomInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(room);
	if (roomInfo) {
		return GetGameNodeIdFromGuid(roomInfo->guid());
	}
	else {
		LOG_ERROR << "RoomInfo not found for entity: " << entt::to_integral(room);
		return kInvalidNodeId;
	}
}

// Generate unique room ID
uint32_t RoomUtil::GenRoomGuid() {
	uint32_t roomId = nodeSequence.Generate();
	while (tlsRegistryManager.roomRegistry.valid(entt::entity{ roomId })) {
		roomId = nodeSequence.Generate();
	}
	LOG_INFO << "Generated new room ID: " << roomId;
	return roomId;
}

// Get total number of rooms associated with a specific configuration ID
std::size_t RoomUtil::GetRoomsSize(uint32_t roomConfigId) {
	std::size_t roomSize = 0;
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	for (auto node : registry.view<NodeNodeComp>()) {
		auto& nodeRoomComp = registry.get<NodeNodeComp>(node);
		roomSize += nodeRoomComp.GetScenesByConfig(roomConfigId).size();
	}
	LOG_TRACE << "Total rooms size for config ID " << roomConfigId << ": " << roomSize;
	return roomSize;
}

// Get total number of rooms in the registry
std::size_t RoomUtil::GetRoomsSize() {
	std::size_t totalRooms = tlsRegistryManager.roomRegistry.storage<RoomInfoPBComponent>().size();
	LOG_TRACE << "Total rooms in the registry: " << totalRooms;
	return totalRooms;
}

// Check if room registry is empty
bool RoomUtil::IsRoomEmpty() {
	bool isEmpty = tlsRegistryManager.roomRegistry.storage<RoomInfoPBComponent>().empty();
	LOG_TRACE << "Room registry empty: " << (isEmpty ? "true" : "false");
	return isEmpty;
}

// Check if there are non-empty room lists for a specific configuration
bool RoomUtil::ConfigRoomListNotEmpty(uint32_t roomConfigId) {
	auto& roomNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	for (auto nodeEid : roomNodeRegistry.view<NodeNodeComp>()) {
		auto& nodeRoomComp = roomNodeRegistry.get<NodeNodeComp>(nodeEid);
		if (!nodeRoomComp.GetScenesByConfig(roomConfigId).empty()) {
			LOG_TRACE << "Non-empty room list found for config ID: " << roomConfigId;
			return true;
		}
	}
	LOG_TRACE << "No non-empty room list found for config ID: " << roomConfigId;
	return false;
}

// ✅ 2. 多节点负载均衡
// 你在 CompelPlayerChangeRoom 中如果找不到房间就直接创建了：
//
// cpp
// 复制
// 编辑
// roomEntity = CreateRoomOnRoomNode(p);
// 更好的做法是考虑负载均衡，比如：
//
// 查找资源空闲的节点（CPU / 玩家少）。
//
// 优先选择已有房间，再决定是否创建。

// Create a new room associated with a game node
entt::entity RoomUtil::CreateRoomOnRoomNode(const CreateRoomOnNodeRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for creating room";
		return entt::null;
	}

	RoomInfoPBComponent roomInfo(param.roomInfo);
	if (roomInfo.guid() <= 0) {
		roomInfo.set_guid(GenRoomGuid());
	}

	const auto room = TryCreateEntity(tlsRegistryManager.roomRegistry, entt::entity{ roomInfo.guid() });
	if (room == entt::null) {
		LOG_ERROR << "Failed to create room in registry";
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

	LOG_INFO << "Created new room with ID: " << tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(room).guid();
	return room;
}

// Destroy a room
void RoomUtil::DestroyRoom(const DestroyRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for destroying room";
		return;
	}

	auto* pServerComp = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<NodeNodeComp>(param.node);
	if (!pServerComp) {
		LOG_ERROR << "ServerComp not found for node";
		return;
	}

	auto* roomInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(param.room);
	if (!roomInfo) {
		LOG_ERROR << "RoomInfo not found for room";
		return;
	}

	OnRoomDestroyed roomDestroyedEvent;
	roomDestroyedEvent.set_entity(entt::to_integral(param.room));
	dispatcher.trigger(roomDestroyedEvent);

	pServerComp->RemoveScene(param.room);

	LOG_INFO << "Destroyed room with ID: " << roomInfo->guid();
}

// Handle server node destruction
void RoomUtil::HandleDestroyRoomNode(entt::entity node) {
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	auto& nodeRoomComp = registry.get<NodeNodeComp>(node);
	auto roomList = nodeRoomComp.GetSceneLists();

	// Destroy all rooms associated with the server node
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


// Check if a player can enter a room
uint32_t RoomUtil::CheckPlayerEnterRoom(const EnterRoomParam& param) {
	if (!tlsRegistryManager.roomRegistry.valid(param.room)) {
		LOG_ERROR << "Invalid room entity when checking player enter room - Room ID: " << entt::to_integral(param.room);
		return kInvalidEnterSceneParameters;
	}

	auto* roomInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(param.room);
	if (!roomInfo) {
		LOG_ERROR << "RoomInfo not found when checking player enter room - Room ID: " << entt::to_integral(param.room);
		return kInvalidEnterSceneParameters;
	}

	auto creatorId = tlsRegistryManager.actorRegistry.get<Guid>(param.enter);
	if (roomInfo->creators().find(creatorId) == roomInfo->creators().end()) {
		LOG_WARN << "Player cannot enter room due to creator restriction - Room ID: " << entt::to_integral(param.room);
		return kCheckEnterSceneCreator;
	}

	return kSuccess;
}


// Check if room player size limits are respected
uint32_t RoomUtil::HasRoomSlot(entt::entity room) {
	auto& roomPlayers = tlsRegistryManager.roomRegistry.get<RoomPlayers>(room);

	if (roomPlayers.size() >= kMaxRoomPlayer) {
		LOG_WARN << "Room player size limit exceeded - Room ID: " << entt::to_integral(room);
		return kEnterSceneNotFull;
	}

	auto* gsPlayerInfo = tlsRegistryManager.roomRegistry.try_get<GameNodePlayerInfoPtrPBComponent>(room);
	if (!gsPlayerInfo) {
		LOG_ERROR << "GameNodePlayerInfoPtr not found for room - Room ID: " << entt::to_integral(room);
		return kEnterSceneGsInfoNull;
	}

	if ((*gsPlayerInfo)->player_size() >= kMaxServerPlayerSize) {
		LOG_WARN << "Game node player size limit exceeded - Room ID: " << entt::to_integral(room);
		return kEnterSceneGsFull;
	}

	return kSuccess;
}

// Enter a player into a room
void RoomUtil::EnterRoom(const EnterRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering room";
		return;
	}

	if (!tlsRegistryManager.roomRegistry.valid(param.room))
	{
		LOG_ERROR << "Invalid room entity when entering room - Room ID: " << entt::to_integral(param.room);
		return;
	}

	if (!tlsRegistryManager.actorRegistry.valid(param.enter))
	{
		LOG_ERROR << "Invalid player entity when entering room - Player : " << entt::to_integral(param.enter);
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
		LOG_INFO << "Player entered room - Player GUID: " << tlsRegistryManager.actorRegistry.get<Guid>(param.enter) << ", Room ID: " << entt::to_integral(param.room);
	}
}


// Enter a player into the default room
void RoomUtil::EnterDefaultRoom(const EnterDefaultRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering default room";
		return;
	}

	// Get a room that is not full from the NodeRoomSystem
	auto defaultRoom = NodeSceneSystem::FindNotFullRoom({});

	// Enter the player into the retrieved default room
	EnterRoom({ defaultRoom, param.enter });

	// Log the entry into the default room
	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.enter))
	{
		LOG_INFO << "Player entered default room - Player GUID: " << tlsRegistryManager.actorRegistry.get<Guid>(param.enter) << ", Room ID: " << entt::to_integral(defaultRoom);
	}
}

// Leave a player from a room
void RoomUtil::LeaveRoom(const LeaveRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when leaving room";
		return;
	}

	if (!tlsRegistryManager.actorRegistry.valid(param.leaver)) {
		LOG_ERROR << "Invalid player entity when leaving room - Player GUID: " << entt::to_integral(param.leaver);
		return;
	}

	auto roomEntityComp = tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(param.leaver);
	if (nullptr == roomEntityComp)
	{
		LOG_ERROR << "RoomEntityComp not found for player when leaving room - Player : " << entt::to_integral(param.leaver);
		return;
	}

	auto roomEntity = roomEntityComp->roomEntity;
	if (!tlsRegistryManager.roomRegistry.valid(roomEntity)) {
		LOG_ERROR << "Invalid room entity when leaving room - Player : " << entt::to_integral(param.leaver);
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
		LOG_INFO << "Player left room - Player GUID: " << tlsRegistryManager.actorRegistry.get<Guid>(param.leaver) << ", Room ID: " << entt::to_integral(roomEntity);
	}
}

// 这里只处理了同gs,如果是跨gs的room切换，应该别的地方处理
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
