#include "room.h"
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
#include <modules/scene/comp/room_node_comp.h>
#include <modules/scene/system/room_common.h>
#include "room_node_selector.h"
#include "room_selector.h"

// Constants
static constexpr std::size_t kMaxRoomPlayer = 1000;


void AddMainRoomToNodeComponent(entt::registry& reg, const entt::entity node) {
	LOG_TRACE << "Adding main room node components for entity: " << entt::to_integral(node);
	reg.emplace<MainRoomNode>(node);
	reg.emplace<RoomRegistryComp>(node);
	reg.emplace<RoomNodePlayerStatsPtrPbComponent>(node, std::make_shared<GameNodePlayerInfoPBComponent>());
}

// RoomUtil implementation
RoomSystem::RoomSystem() {
	LOG_TRACE << "RoomSystem constructor called";
	ClearAllRoomData();
}

RoomSystem::~RoomSystem() {
	LOG_TRACE << "RoomSystem destructor called";
	ClearAllRoomData();
}

void RoomSystem::ClearAllRoomData() {
	LOG_TRACE << "Clearing room system data";
	tlsRegistryManager.roomRegistry.clear();
	tlsRegistryManager.actorRegistry.clear();
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
}

// Get game node ID associated with a room entity
NodeId RoomSystem::GetGameNodeIdFromRoomEntity(entt::entity room) {
	auto* roomInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(room);
	if (roomInfo) {
		return RoomCommon::GetGameNodeIdFromGuid(roomInfo->guid());
	}
	else {
		LOG_ERROR << "RoomInfo not found for entity: " << entt::to_integral(room);
		return kInvalidNodeId;
	}
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

// Handle server node destruction
void RoomSystem::HandleDestroyRoomNode(entt::entity node) {
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	auto& nodeRoomComp = registry.get<RoomRegistryComp>(node);
	auto roomList = nodeRoomComp.GetRoomMap();

	// Destroy all rooms associated with the server node
	for (auto& confIdRoomList : roomList | std::views::values) {
		for (auto room : confIdRoomList) {
			RoomCommon::DestroyRoom({ node, room });
		}
	}

	// Destroy the server node itself
	Destroy(registry, node);

	// Log server destruction
	LOG_INFO << "Destroyed server with ID: " << entt::to_integral(node);
}

// Enter a player into the default room
void RoomSystem::EnterDefaultRoom(const EnterDefaultRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering default room";
		return;
	}

	// Get a room that is not full from the NodeRoomSystem
	auto defaultRoom = RoomNodeSelector::SelectAvailableRoom({});

	// Enter the player into the retrieved default room
	RoomCommon::EnterRoom({ defaultRoom, param.enter });

	// Log the entry into the default room
	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.enter))
	{
		LOG_INFO << "Player entered default room - Player GUID: " << tlsRegistryManager.actorRegistry.get<Guid>(param.enter) << ", Room ID: " << entt::to_integral(defaultRoom);
	}
}

// 这里只处理了同gs,如果是跨gs的room切换，应该别的地方处理
void RoomSystem::CompelPlayerChangeRoom(const CompelChangeRoomParam& param) {
	// ✅ 使用 FindOrCreateRoom 替代原始杂糅逻辑
	entt::entity roomEntity = RoomSystem::FindOrCreateRoom(param.roomConfId);

	RoomCommon::LeaveRoom({ param.player });

	if (roomEntity == entt::null) {
		EnterDefaultRoom({ param.player });
		return;
	}

	RoomCommon::EnterRoom({ roomEntity, param.player });
}



// Replace a crashed server node with a new node
void RoomSystem::ReplaceCrashRoomNode(entt::entity crashNode, entt::entity destNode) {
	auto& roomRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto& crashNodeRoom = roomRegistry.get<RoomRegistryComp>(crashNode);
	auto roomList = crashNodeRoom.GetRoomMap();

	for (auto& confIdRoomList : roomList | std::views::values) {
		for (auto room : confIdRoomList) {
			auto* roomInfo = tlsRegistryManager.roomRegistry.try_get<RoomInfoPBComponent>(room);
			if (!roomInfo) {
				continue;
			}
			CreateRoomOnNodeRoomParam p{ .node = destNode };
			p.roomInfo.set_scene_confid(roomInfo->scene_confid());
			RoomCommon::CreateRoomOnRoomNode(p);
		}
	}

	Destroy(roomRegistry, crashNode);
	LOG_INFO << "Replaced crashed server with new node: " << entt::to_integral(destNode);
}


entt::entity RoomSystem::FindOrCreateRoom(uint32_t roomConfId) {
	// 选择最优服务器节点
	entt::entity node = SelectBestNodeForRoom(roomConfId);
	if (node == entt::null) {
		LOG_ERROR << "FindOrCreateRoom: Failed to select a room node for roomConfId = " << roomConfId;
		return entt::null;
	}

	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto& nodeRoomComp = registry.get<RoomRegistryComp>(node);

	// 查找已有房间
	entt::entity room = RoomSelector::SelectRoomWithMinPlayers(nodeRoomComp, roomConfId);
	if (room != entt::null) {
		return room;
	}

	// 创建新房间
	CreateRoomOnNodeRoomParam createParam{ .node = node };
	createParam.roomInfo.set_scene_confid(roomConfId);

	room = RoomCommon::CreateRoomOnRoomNode(createParam);
	if (room == entt::null) {
		LOG_ERROR << "FindOrCreateRoom: Failed to create room for roomConfId = " << roomConfId;
	}

	return room;
}

entt::entity RoomSystem::SelectBestNodeForRoom(uint32_t roomConfId) {
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	entt::entity bestNode = entt::null;
	std::size_t minPlayerCount = std::numeric_limits<std::size_t>::max();

	for (auto node : registry.view<RoomRegistryComp, RoomNodePlayerStatsPtrPbComponent>()) {
		const auto& nodeComp = registry.get<RoomRegistryComp>(node);
		const auto& playerInfoPtr = registry.get<RoomNodePlayerStatsPtrPbComponent>(node);
		if (!playerInfoPtr) continue;

		// 如果该节点已经有该配置的房间，优先考虑
		auto existingRooms = nodeComp.GetRoomsByConfig(roomConfId);
		if (!existingRooms.empty()) {
			return node;
		}

		// 否则看节点的整体玩家数量是否最小
		std::size_t playerSize = playerInfoPtr->player_size();
		if (playerSize < minPlayerCount) {
			minPlayerCount = playerSize;
			bestNode = node;
		}
	}

	if (bestNode == entt::null) {
		LOG_WARN << "SelectBestNodeForRoom: No suitable node found for roomConfId = " << roomConfId;
	}

	return bestNode;
}
