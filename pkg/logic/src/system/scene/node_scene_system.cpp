#include "node_scene_system.h"
#include <ranges>
#include "comp/node_scene.h"
#include "thread_local/storage.h"
#include "proto/logic/component/gs_node_comp.pb.h"
#include "muduo/base/Logging.h"

using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;

template <typename ServerType>
entt::entity FindSceneWithMinPlayerCountTemplate(const GetSceneParam& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfId;
	entt::entity bestNode{ entt::null };
	std::size_t minServerPlayerSize = UINT64_MAX;

	for (auto entity : tls.game_node_registry.view<ServerType>()) {
		const auto& serverComp = tls.game_node_registry.get<ServerComp>(entity);

		if (!serverComp.IsStateNormal() ||
			serverComp.GetSceneListByConfig(sceneConfigId).empty() ||
			serverComp.GetServerPressureState() != filterStateParam.nodePressureState) {
			continue;
		}

		auto serverPlayerSize = (*tls.game_node_registry.get<GameNodePlayerInfoPtr>(entity)).player_size();

		if (serverPlayerSize >= minServerPlayerSize || serverPlayerSize >= kMaxServerPlayerSize) {
			continue;
		}

		bestNode = entity;
		minServerPlayerSize = serverPlayerSize;
	}

	if (entt::null == bestNode) {
		LOG_WARN << "FindSceneWithMinPlayerCountTemplate: No suitable node found for scene configuration ID: " << sceneConfigId;
		return entt::null;
	}

	const auto& serverComps = tls.game_node_registry.get<ServerComp>(bestNode);
	auto bestScene = serverComps.GetSceneWithMinPlayerCountByConfigId(sceneConfigId);

	if (bestScene == entt::null) {
		LOG_WARN << "FindSceneWithMinPlayerCountTemplate: No scene found with minimum player count for node: " << entt::to_integral(bestNode);
	}

	return bestScene;
}

template <typename ServerType>
entt::entity FindNotFullSceneTemplate(const GetSceneParam& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfId;
	entt::entity bestNode{ entt::null };

	for (auto entity : tls.game_node_registry.view<ServerType>()) {
		if (const auto& serverComp = tls.game_node_registry.get<ServerComp>(entity);
			!serverComp.IsStateNormal() ||
			serverComp.GetSceneListByConfig(sceneConfigId).empty() ||
			serverComp.GetServerPressureState() != filterStateParam.nodePressureState) {
			continue;
		}

		auto serverPlayerSize = (*tls.game_node_registry.get<GameNodePlayerInfoPtr>(entity)).player_size();

		if (serverPlayerSize >= kMaxServerPlayerSize) {
			continue;
		}

		bestNode = entity;
		break;
	}

	if (entt::null == bestNode) {
		LOG_WARN << "FindNotFullSceneTemplate: No suitable node found for scene configuration ID: " << sceneConfigId;
		return entt::null;
	}

	entt::entity bestScene{ entt::null };
	const auto& serverComps = tls.game_node_registry.get<ServerComp>(bestNode);

	for (const auto& sceneIt : serverComps.GetSceneListByConfig(sceneConfigId)) {
		auto scenePlayerSize = tls.scene_registry.get<ScenePlayers>(sceneIt).size();

		if (scenePlayerSize >= kMaxScenePlayerSize) {
			continue;
		}

		bestScene = sceneIt;
		break;
	}

	if (bestScene == entt::null) {
		LOG_WARN << "FindNotFullSceneTemplate: No scene found that is not full for node: " << entt::to_integral(bestNode);
	}

	return bestScene;
}

entt::entity NodeSceneSystem::FindSceneWithMinPlayerCount(const GetSceneParam& param) {
	constexpr GetSceneFilterParam filterParam;

	auto bestScene = FindSceneWithMinPlayerCountTemplate<MainSceneServer>(param, filterParam);
	if (bestScene != entt::null) {
		LOG_INFO << "FindSceneWithMinPlayerCount: Found scene with minimum player count, Scene ID: " << entt::to_integral(bestScene);
		return bestScene;
	}

	LOG_WARN << "FindSceneWithMinPlayerCount: No scene found with minimum player count";
	return FindSceneWithMinPlayerCountTemplate<MainSceneServer>(param, filterParam);
}

entt::entity NodeSceneSystem::FindNotFullScene(const GetSceneParam& param) {
	GetSceneFilterParam filterParam;

	auto bestScene = FindNotFullSceneTemplate<MainSceneServer>(param, filterParam);
	if (bestScene != entt::null) {
		LOG_INFO << "FindNotFullScene: Found scene that is not full, Scene ID: " << entt::to_integral(bestScene);
		return bestScene;
	}

	LOG_WARN << "FindNotFullScene: No scene found that is not full";
	filterParam.nodePressureState = NodePressureState::kPressure;
	return FindNotFullSceneTemplate<MainSceneServer>(param, filterParam);
}

void NodeSceneSystem::SetNodePressure(entt::entity node) {
	auto* const serverComp = tls.game_node_registry.try_get<ServerComp>(node);

	if (nullptr == serverComp) {
		LOG_ERROR << "SetNodePressure: ServerComp not found for node: " << entt::to_integral(node);
		return;
	}

	serverComp->SetNodePressureState(NodePressureState::kPressure);
	LOG_INFO << "SetNodePressure: Node entered pressure state, Node ID: " << entt::to_integral(node);
}

void NodeSceneSystem::ClearNodePressure(entt::entity node) {
	auto* const serverComp = tls.game_node_registry.try_get<ServerComp>(node);

	if (nullptr == serverComp) {
		LOG_ERROR << "ClearNodePressure: ServerComp not found for node: " << entt::to_integral(node);
		return;
	}

	serverComp->SetNodePressureState(NodePressureState::kNoPressure);
	LOG_INFO << "ClearNodePressure: Node exited pressure state, Node ID: " << entt::to_integral(node);
}

void NodeSceneSystem::SetNodeState(entt::entity node, NodeState state) {
	auto* const tryServerComp = tls.game_node_registry.try_get<ServerComp>(node);

	if (nullptr == tryServerComp) {
		LOG_ERROR << "SetNodeState: ServerComp not found for node: " << entt::to_integral(node);
		return;
	}

	tryServerComp->SetState(state);
	LOG_INFO << "SetNodeState: Node state set successfully, Node ID: " << entt::to_integral(node) << ", State: " << static_cast<int>(state);
}
