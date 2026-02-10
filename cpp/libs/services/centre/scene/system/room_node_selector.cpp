#include "room_node_selector.h"
#include <ranges>
#include "modules/scene/comp/room_node_comp.h"

#include "proto/logic/component/game_node_comp.pb.h"
#include "proto/common/base/node.pb.h"
#include "muduo/base/Logging.h"
#include "threading/node_context_manager.h"
#include "room_selector.h"

// 常量定义
constexpr std::size_t kInvalidPlayerCount = std::numeric_limits<std::size_t>::max();

// 选择匹配的节点
template <typename ServerType>
entt::entity FindBestNode(uint32_t configId, NodePressureState desiredPressure, bool preferLeastLoaded) {
	entt::entity bestNode{ entt::null };
	std::size_t bestLoad = kInvalidPlayerCount;

	auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::RoomNodeService);

	for (auto entity : nodeRegistry.view<ServerType>()) {
		const auto& roomRegistry = nodeRegistry.get<RoomRegistryComp>(entity);

		// 判断节点是否有效
		if (!nodeRegistry.get_or_emplace<NodeStateComp>(entity, NodeState::kNormal).IsNormal() ||
			roomRegistry.GetRoomsByConfig(configId).empty() ||
			nodeRegistry.get_or_emplace<NodePressureComp>(entity, NodePressureState::kNoPressure) != desiredPressure) {
			continue;
		}

		std::size_t playerCount = nodeRegistry.get<RoomNodePlayerStatsPtrPbComponent>(entity)->player_size();

		// 若不考虑负载，则只需找到第一个合适节点
		if (!preferLeastLoaded) {
			if (playerCount < kMaxServerPlayerSize) {
				return entity;
			}
			continue;
		}

		// 寻找负载最低的节点
		if (playerCount < bestLoad && playerCount < kMaxServerPlayerSize) {
			bestNode = entity;
			bestLoad = playerCount;

			if (playerCount == 0) break; // 已是最优
		}
	}

	if (bestNode == entt::null) {
		LOG_WARN << "No suitable node found for room config ID: " << configId;
	}

	return bestNode;
}

// 在节点中选择玩家最少的房间
entt::entity FindRoomWithMinPlayers(entt::entity nodeEntity, uint32_t configId) {
	auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::RoomNodeService);
	const auto& roomRegistry = nodeRegistry.get<RoomRegistryComp>(nodeEntity);

	auto room = RoomSelectorSystem::SelectRoomWithMinPlayers(roomRegistry, configId);

	if (room == entt::null) {
		LOG_WARN << "No suitable room found with minimum players on node: " << entt::to_integral(nodeEntity);
	}

	return room;
}

// 在节点中选择一个未满的房间
entt::entity FindFirstAvailableRoom(entt::entity nodeEntity, uint32_t configId) {
	auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::RoomNodeService);
	const auto& roomRegistry = nodeRegistry.get<RoomRegistryComp>(nodeEntity);

	for (const auto& room : roomRegistry.GetRoomsByConfig(configId)) {
		std::size_t roomPlayerCount = tlsRegistryManager.roomRegistry.get<RoomPlayers>(room).size();
		if (roomPlayerCount < kMaxPlayersPerRoom) {
			return room;
		}
	}

	LOG_WARN << "No available (not full) room found on node: " << entt::to_integral(nodeEntity);
	return entt::null;
}

// 外部接口：选择负载最小的房间
entt::entity RoomNodeSelectorSystem::SelectLeastLoadedRoom(const GetRoomParams& param) {
	constexpr GetRoomFilterParam defaultFilter;

	auto node = FindBestNode<MainRoomNode>(param.roomConfigurationId, defaultFilter.nodePressureState, true);
	if (node == entt::null) return entt::null;

	return FindRoomWithMinPlayers(node, param.roomConfigurationId);
}

// 外部接口：选择第一个可用房间
entt::entity RoomNodeSelectorSystem::SelectAvailableRoom(const GetRoomParams& param) {
	GetRoomFilterParam filter;

	auto node = FindBestNode<MainRoomNode>(param.roomConfigurationId, filter.nodePressureState, false);
	if (node != entt::null) {
		auto room = FindFirstAvailableRoom(node, param.roomConfigurationId);
		if (room != entt::null) return room;
	}

	// 尝试在高压力节点中再查一次
	filter.nodePressureState = NodePressureState::kPressure;

	node = FindBestNode<MainRoomNode>(param.roomConfigurationId, filter.nodePressureState, false);
	if (node != entt::null) {
		return FindFirstAvailableRoom(node, param.roomConfigurationId);
	}

	LOG_WARN << "No available room found even under pressure for config ID: " << param.roomConfigurationId;
	return entt::null;
}
