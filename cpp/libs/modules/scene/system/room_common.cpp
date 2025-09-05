#include "room_common.h"
#include <core/utils/id/node_id_generator.h>
#include <muduo/base/Logging.h>
#include <registry_manager.h>
#include <node_context_manager.h>
#include <proto/common/common.pb.h>
#include <scene/comp/node_scene_comp.h>
#include <table/proto/tip/scene_error_tip.pb.h>
#include "room_system.h"
#include <table/proto/tip/common_error_tip.pb.h>
#include <services/centre/node/comp/game_node_comp.h>
#include <proto/logic/event/scene_event.pb.h>
#include <dispatcher_manager.h>
#include <scene/comp/scene_comp.h>

static constexpr std::size_t kMaxRoomPlayer1 = 1000;

thread_local TransientNode12BitCompositeIdGenerator  nodeRoomSequence; // Sequence for generating node IDs

NodeId RoomCommon::GetGameNodeIdFromGuid(uint64_t room_id)
{
	return nodeRoomSequence.node_id(static_cast<NodeId>(room_id));
}

entt::entity RoomCommon::GetRoomNodeEntityId(uint64_t room_id)
{
	return entt::entity{ nodeRoomSequence.node_id(static_cast<NodeId>(room_id)) };
}

void RoomCommon::SetSequenceNodeId(const uint32_t node_id) { nodeRoomSequence.set_node_id(node_id); }

void RoomCommon::ClearAllRoomData()
{
	LOG_TRACE << "Clearing room system data";
	tlsRegistryManager.roomRegistry.clear();
	tlsRegistryManager.actorRegistry.clear();
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
}

uint32_t RoomCommon::GenRoomGuid() {
	uint32_t roomId = nodeRoomSequence.Generate();
	while (tlsRegistryManager.roomRegistry.valid(entt::entity{ roomId })) {
		roomId = nodeRoomSequence.Generate();
	}
	LOG_INFO << "Generated new room ID: " << roomId;
	return roomId;
}

bool RoomCommon::IsRoomEmpty() {
	bool isEmpty = tlsRegistryManager.roomRegistry.storage<RoomInfoPBComponent>().empty();
	LOG_TRACE << "Room registry empty: " << (isEmpty ? "true" : "false");
	return isEmpty;
}


// Get total number of rooms associated with a specific configuration ID
std::size_t RoomCommon::GetRoomsSize(uint32_t roomConfigId) {
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
std::size_t RoomCommon::GetRoomsSize() {
	std::size_t totalRooms = tlsRegistryManager.roomRegistry.storage<RoomInfoPBComponent>().size();
	LOG_TRACE << "Total rooms in the registry: " << totalRooms;
	return totalRooms;
}

bool RoomCommon::ConfigRoomListNotEmpty(uint32_t roomConfigId) {
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

uint32_t RoomCommon::CheckPlayerEnterRoom(const EnterRoomParam& param) {
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


uint32_t RoomCommon::HasRoomSlot(entt::entity room) {
	auto& roomPlayers = tlsRegistryManager.roomRegistry.get<RoomPlayers>(room);

	if (roomPlayers.size() >= kMaxRoomPlayer1) {
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


void RoomCommon::EnterRoom(const EnterRoomParam& param) {
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


// Leave a player from a room
void RoomCommon::LeaveRoom(const LeaveRoomParam& param) {
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