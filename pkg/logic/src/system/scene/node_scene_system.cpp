#include "node_scene_system.h"
#include <ranges>
#include "comp/node_scene.h"
#include "thread_local/storage.h"
#include "proto/logic/component/gs_node_comp.pb.h"


using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;

template <typename ServerType>
entt::entity GetSceneOnMinPlayerSizeNodeT(const GetSceneParam& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfId_;
	entt::entity node{ entt::null };
	std::size_t minServerPlayerSize = UINT64_MAX;

	for (auto entity : tls.game_node_registry.view<ServerType>()) {
		if (const auto& serverComp = tls.game_node_registry.get<ServerComp>(entity);
			!serverComp.IsStateNormal() ||
			serverComp.GetSceneListByConfig(sceneConfigId).empty() ||
			serverComp.GetServerPressureState() != filterStateParam.nodePressureState) {
			continue;
		}

		auto serverPlayerSize = (*tls.game_node_registry.get<GameNodePlayerInfoPtr>(entity)).player_size();

		if (serverPlayerSize >= minServerPlayerSize || serverPlayerSize >= kMaxServerPlayerSize) {
			continue;
		}

		node = entity;
		minServerPlayerSize = serverPlayerSize;
	}

	if (entt::null == node) {
		return entt::null;
	}

	const auto& serverComps = tls.game_node_registry.get<ServerComp>(node);
	return serverComps.GetMinPlayerSizeSceneByConfigId(sceneConfigId);
}

template <typename ServerType>
entt::entity GetNotFullSceneT(const GetSceneParam& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfId_;
	entt::entity server{ entt::null };

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

		server = entity;
		break;
	}

	if (entt::null == server) {
		return entt::null;
	}

	entt::entity scene{ entt::null };
	const auto& serverComps = tls.game_node_registry.get<ServerComp>(server);

	for (const auto& sceneIt : serverComps.GetSceneListByConfig(sceneConfigId)) {
		if (const auto scenePlayerSize = tls.scene_registry.get<ScenePlayers>(sceneIt).size();
			scenePlayerSize >= kMaxScenePlayerSize) {
			continue;
		}

		scene = sceneIt;
		break;
	}

	return scene;
}

entt::entity NodeSceneSystem::GetSceneOnMinPlayerSizeNode(const GetSceneParam& param) {
	constexpr GetSceneFilterParam getSceneFilterParam;

	if (const auto scene = GetSceneOnMinPlayerSizeNodeT<MainSceneServer>(param, getSceneFilterParam); entt::null != scene) {
		return scene;
	}

	return GetSceneOnMinPlayerSizeNodeT<MainSceneServer>(param, getSceneFilterParam);
}

entt::entity NodeSceneSystem::GetNotFullScene(const GetSceneParam& param) {
	GetSceneFilterParam getSceneFilterParam;

	if (const auto sceneEntity = GetNotFullSceneT<MainSceneServer>(param, getSceneFilterParam);
		entt::null != sceneEntity) {
		return sceneEntity;
	}

	getSceneFilterParam.nodePressureState = NodePressureState::kPressure;
	return GetNotFullSceneT<MainSceneServer>(param, getSceneFilterParam);
}

void NodeSceneSystem::NodeEnterPressure(entt::entity node) {
	auto* const serverComp = tls.game_node_registry.try_get<ServerComp>(node);

	if (nullptr == serverComp) {
		return;
	}

	serverComp->SetNodePressureState(NodePressureState::kPressure);
}

void NodeSceneSystem::NodeEnterNoPressure(entt::entity node) {
	auto* const serverComp = tls.game_node_registry.try_get<ServerComp>(node);

	if (nullptr == serverComp) {
		return;
	}

	serverComp->SetNodePressureState(NodePressureState::kNoPressure);
}

void NodeSceneSystem::SetNodeState(entt::entity node, NodeState nodeState) {
	auto* const tryServerComp = tls.game_node_registry.try_get<ServerComp>(node);

	if (nullptr == tryServerComp) {
		return;
	}

	tryServerComp->SetNodeState(nodeState);
}
