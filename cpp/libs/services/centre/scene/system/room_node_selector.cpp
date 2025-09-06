#include "room_node_selector.h"
#include <ranges>
#include "modules/scene/comp/room_node_comp.h"

#include "proto/logic/component/game_node_comp.pb.h"
#include "proto/common/node.pb.h"
#include "muduo/base/Logging.h"
#include "threading/node_context_manager.h"
#include "room_selector.h"

template <typename ServerType>
entt::entity SelectLeastLoadedRoomTemplate(const GetRoomParams& param, const GetRoomFilterParam& filterStateParam) {
	auto roomConfigId = param.roomConfigurationId;
	entt::entity bestNode{ entt::null };
	std::size_t minServerPlayerSize = UINT64_MAX;

	auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	for (auto entity : nodeRegistry.view<ServerType>()) {
		const auto& roomRegistryComp = nodeRegistry.get<RoomRegistryComp>(entity);

		if (!nodeRegistry.get_or_emplace<NodeStateComp>(entity, NodeState::kNormal).IsNormal() ||
			roomRegistryComp.GetRoomsByConfig(roomConfigId).empty() ||
			nodeRegistry.get_or_emplace<NodePressureComp>(entity, NodePressureState::kNoPressure) != filterStateParam.nodePressureState) {
			continue;
		}

		auto nodePlayerSize = (*nodeRegistry.get<RoomNodePlayerStatsPtrPbComponent>(entity)).player_size();
		if (nodePlayerSize == 0) {
			bestNode = entity;
			minServerPlayerSize = nodePlayerSize;
			break;
		}

		if (nodePlayerSize >= minServerPlayerSize || nodePlayerSize >= kMaxServerPlayerSize) {
			continue;
		}

		bestNode = entity;
		minServerPlayerSize = nodePlayerSize;
	}

	if (entt::null == bestNode) {
		LOG_WARN << "No suitable node found for scene configuration ID: " << roomConfigId;
		return entt::null;
	}

	const auto& roomRegistryComp = nodeRegistry.get<RoomRegistryComp>(bestNode);
	auto roomScene = RoomSelector::SelectRoomWithMinPlayers(roomRegistryComp, roomConfigId);

	if (roomScene == entt::null) {
		LOG_WARN << "No scene found with minimum player count for node: " << entt::to_integral(bestNode);
	}

	return roomScene;
}

template <typename ServerType>
entt::entity SelectAvailableRoomRoomTemplate(const GetRoomParams& param, const GetRoomFilterParam& filterStateParam) {
	auto roomConfigId = param.roomConfigurationId;
	entt::entity bestNode{ entt::null };
	auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	for (auto entity : nodeRegistry.view<ServerType>()) {
		if (const auto& roomRegistryComp = nodeRegistry.get<RoomRegistryComp>(entity);
			!nodeRegistry.get_or_emplace<NodeStateComp>(entity, NodeState::kNormal).IsNormal() ||
			roomRegistryComp.GetRoomsByConfig(roomConfigId).empty() ||
			nodeRegistry.get_or_emplace<NodePressureComp>(entity, NodePressureState::kNoPressure) != filterStateParam.nodePressureState) {
			continue;
		}

		auto nodePlayerSize = (*nodeRegistry.get<RoomNodePlayerStatsPtrPbComponent>(entity)).player_size();

		if (nodePlayerSize >= kMaxServerPlayerSize) {
			continue;
		}

		bestNode = entity;
		break;
	}

	if (entt::null == bestNode) {
		LOG_WARN << "No suitable node found for scene configuration ID: " << roomConfigId;
		return entt::null;
	}

	entt::entity bestRoom{ entt::null };
	const auto& roomRegistryComp = nodeRegistry.get<RoomRegistryComp>(bestNode);

	for (const auto& roomIt : roomRegistryComp.GetRoomsByConfig(roomConfigId)) {
		auto roomPlayerSize = tlsRegistryManager.roomRegistry.get<RoomPlayers>(roomIt).size();
		if (roomPlayerSize >= kMaxPlayersPerRoom) {
			continue;
		}

		bestRoom = roomIt;
		break;
	}

	if (bestRoom == entt::null) {
		LOG_WARN << "No scene found that is not full for node: " << entt::to_integral(bestNode);
	}

	return bestRoom;
}

entt::entity RoomNodeSelector::SelectLeastLoadedRoom(const GetRoomParams& param) {
	constexpr GetRoomFilterParam filterParam;

	auto bestRoom = SelectLeastLoadedRoomTemplate<MainRoomNode>(param, filterParam);
	if (bestRoom != entt::null) {
		return bestRoom;
	}

	LOG_WARN << "No room found with minimum player count";
	return SelectLeastLoadedRoomTemplate<MainRoomNode>(param, filterParam);
}

entt::entity RoomNodeSelector::SelectAvailableRoom(const GetRoomParams& param) {
	GetRoomFilterParam filterParam;

	auto bestRoom = SelectAvailableRoomRoomTemplate<MainRoomNode>(param, filterParam);
	if (bestRoom != entt::null) {
		return bestRoom;
	}

	LOG_WARN << "No room found that is not full";
	filterParam.nodePressureState = NodePressureState::kPressure;
	return SelectAvailableRoomRoomTemplate<MainRoomNode>(param, filterParam);
}
