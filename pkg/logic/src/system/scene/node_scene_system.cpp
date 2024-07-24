#include "node_scene_system.h"
#include <ranges>
#include "comp/node_scene.h"
#include "thread_local/storage.h"
#include "proto/logic/component/gs_node_comp.pb.h"


using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;

template <typename ServerType>
entt::entity FindSceneWithMinPlayerCountTemplate(const GetSceneParam& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfId_;
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
		return entt::null;
	}

	const auto& serverComps = tls.game_node_registry.get<ServerComp>(bestNode);
	return serverComps.GetSceneWithMinPlayerCountByConfigId(sceneConfigId);
}

template <typename ServerType>
entt::entity FindNotFullSceneTemplate(const GetSceneParam& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfId_;
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

	return bestScene;
}

entt::entity NodeSceneSystem::FindSceneWithMinPlayerCount(const GetSceneParam& param) {
	constexpr GetSceneFilterParam filterParam;

	auto bestScene = FindSceneWithMinPlayerCountTemplate<MainSceneServer>(param, filterParam);
	if (bestScene != entt::null) {
		return bestScene;
	}

	return FindSceneWithMinPlayerCountTemplate<MainSceneServer>(param, filterParam);
}

entt::entity NodeSceneSystem::FindNotFullScene(const GetSceneParam& param) {
	GetSceneFilterParam filterParam;

	auto bestScene = FindNotFullSceneTemplate<MainSceneServer>(param, filterParam);
	if (bestScene != entt::null) {
		return bestScene;
	}

	filterParam.nodePressureState = NodePressureState::kPressure;
	return FindNotFullSceneTemplate<MainSceneServer>(param, filterParam);
}

void NodeSceneSystem::SetNodePressure(entt::entity node) {
	auto* const serverComp = tls.game_node_registry.try_get<ServerComp>(node);

	if (nullptr == serverComp) {
		return;
	}

	serverComp->SetNodePressureState(NodePressureState::kPressure);
}

void NodeSceneSystem::ClearNodePressure(entt::entity node) {
	auto* const serverComp = tls.game_node_registry.try_get<ServerComp>(node);

	if (nullptr == serverComp) {
		return;
	}

	serverComp->SetNodePressureState(NodePressureState::kNoPressure);
}

void NodeSceneSystem::SetNodeState(entt::entity node, NodeState state) {
	auto* const tryServerComp = tls.game_node_registry.try_get<ServerComp>(node);

	if (nullptr == tryServerComp) {
		return;
	}

	tryServerComp->SetState(state);
}
