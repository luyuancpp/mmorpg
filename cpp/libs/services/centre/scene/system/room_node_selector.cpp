#include "room_node_selector.h"
#include <ranges>
#include "modules/scene/comp/room_node_comp.h"

#include "proto/logic/component/game_node_comp.pb.h"
#include "proto/common/node.pb.h"
#include "muduo/base/Logging.h"
#include "threading/node_context_manager.h"

template <typename ServerType>
entt::entity SelectLeastLoadedSceneTemplate(const GetSceneParams& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfigurationId;
	entt::entity bestNode{ entt::null };
	std::size_t minServerPlayerSize = UINT64_MAX;

	auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	for (auto entity : nodeRegistry.view<ServerType>()) {
		const auto& nodeSceneComp = nodeRegistry.get<NodeRoomComp>(entity);

		if (!nodeRegistry.get_or_emplace<NodeStateComp>(entity, NodeState::kNormal).IsNormal() ||
			nodeSceneComp.GetRoomsByConfig(sceneConfigId).empty() ||
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
		LOG_WARN << "No suitable node found for scene configuration ID: " << sceneConfigId;
		return entt::null;
	}

	const auto& nodeSceneComps = nodeRegistry.get<NodeRoomComp>(bestNode);
	auto bestScene = nodeSceneComps.GetSceneWithMinPlayerCountByConfigId(sceneConfigId);

	if (bestScene == entt::null) {
		LOG_WARN << "No scene found with minimum player count for node: " << entt::to_integral(bestNode);
	}

	return bestScene;
}

template <typename ServerType>
entt::entity SelectAvailableRoomSceneTemplate(const GetSceneParams& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfigurationId;
	entt::entity bestNode{ entt::null };
	auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	for (auto entity : nodeRegistry.view<ServerType>()) {
		if (const auto& nodeSceneComp = nodeRegistry.get<NodeRoomComp>(entity);
			!nodeRegistry.get_or_emplace<NodeStateComp>(entity, NodeState::kNormal).IsNormal() ||
			nodeSceneComp.GetRoomsByConfig(sceneConfigId).empty() ||
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
		LOG_WARN << "No suitable node found for scene configuration ID: " << sceneConfigId;
		return entt::null;
	}

	entt::entity bestScene{ entt::null };
	const auto& nodeSceneComps = nodeRegistry.get<NodeRoomComp>(bestNode);

	for (const auto& sceneIt : nodeSceneComps.GetRoomsByConfig(sceneConfigId)) {
		auto scenePlayerSize = tlsRegistryManager.roomRegistry.get<RoomPlayers>(sceneIt).size();

		if (scenePlayerSize >= kMaxScenePlayerSize) {
			continue;
		}

		bestScene = sceneIt;
		break;
	}

	if (bestScene == entt::null) {
		LOG_WARN << "No scene found that is not full for node: " << entt::to_integral(bestNode);
	}

	return bestScene;
}

entt::entity RoomNodeSelector::SelectLeastLoadedScene(const GetSceneParams& param) {
	constexpr GetSceneFilterParam filterParam;

	auto bestScene = SelectLeastLoadedSceneTemplate<MainRoomNode>(param, filterParam);
	if (bestScene != entt::null) {
		return bestScene;
	}

	LOG_WARN << "No scene found with minimum player count";
	return SelectLeastLoadedSceneTemplate<MainRoomNode>(param, filterParam);
}

entt::entity RoomNodeSelector::SelectAvailableRoomScene(const GetSceneParams& param) {
	GetSceneFilterParam filterParam;

	auto bestScene = SelectAvailableRoomSceneTemplate<MainRoomNode>(param, filterParam);
	if (bestScene != entt::null) {
		return bestScene;
	}

	LOG_WARN << "No scene found that is not full";
	filterParam.nodePressureState = NodePressureState::kPressure;
	return SelectAvailableRoomSceneTemplate<MainRoomNode>(param, filterParam);
}
