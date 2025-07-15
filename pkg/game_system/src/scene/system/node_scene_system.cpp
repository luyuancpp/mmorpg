#include "node_scene_system.h"
#include <ranges>
#include "scene/comp/node_scene_comp.h"
#include "thread_local/storage.h"
#include "proto/logic/component/game_node_comp.pb.h"
#include "proto/common/node.pb.h"
#include "muduo/base/Logging.h"

using GameNodePlayerInfoPtrPBComponent = std::shared_ptr<GameNodePlayerInfoPBComponent>;

template <typename ServerType>
entt::entity FindSceneWithMinPlayerCountTemplate(const GetSceneParams& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfigurationId;
	entt::entity bestNode{ entt::null };
	std::size_t minServerPlayerSize = UINT64_MAX;

	auto& nodeRegistry = tls.GetNodeRegistry(eNodeType::SceneNodeService);

	for (auto entity : nodeRegistry.view<ServerType>()) {
		const auto& nodeSceneComp = nodeRegistry.get<NodeSceneComp>(entity);

		if (!nodeSceneComp.IsStateNormal() ||
			nodeSceneComp.GetScenesByConfig(sceneConfigId).empty() ||
			nodeSceneComp.GetNodePressureState() != filterStateParam.nodePressureState) {
			continue;
		}

		auto nodePlayerSize = (*nodeRegistry.get<GameNodePlayerInfoPtrPBComponent>(entity)).player_size();
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

	const auto& nodeSceneComps = nodeRegistry.get<NodeSceneComp>(bestNode);
	auto bestScene = nodeSceneComps.GetSceneWithMinPlayerCountByConfigId(sceneConfigId);

	if (bestScene == entt::null) {
		LOG_WARN << "No scene found with minimum player count for node: " << entt::to_integral(bestNode);
	}

	return bestScene;
}

template <typename ServerType>
entt::entity FindNotFullSceneTemplate(const GetSceneParams& param, const GetSceneFilterParam& filterStateParam) {
	auto sceneConfigId = param.sceneConfigurationId;
	entt::entity bestNode{ entt::null };
	auto& registry = tls.GetNodeRegistry(eNodeType::SceneNodeService);

	for (auto entity : registry.view<ServerType>()) {
		if (const auto& nodeSceneComp = registry.get<NodeSceneComp>(entity);
			!nodeSceneComp.IsStateNormal() ||
			nodeSceneComp.GetScenesByConfig(sceneConfigId).empty() ||
			nodeSceneComp.GetNodePressureState() != filterStateParam.nodePressureState) {
			continue;
		}

		auto nodePlayerSize = (*registry.get<GameNodePlayerInfoPtrPBComponent>(entity)).player_size();

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
	const auto& nodeSceneComps = registry.get<NodeSceneComp>(bestNode);

	for (const auto& sceneIt : nodeSceneComps.GetScenesByConfig(sceneConfigId)) {
		auto scenePlayerSize = tls.sceneRegistry.get<ScenePlayers>(sceneIt).size();

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

entt::entity NodeSceneSystem::FindSceneWithMinPlayerCount(const GetSceneParams& param) {
	constexpr GetSceneFilterParam filterParam;

	auto bestScene = FindSceneWithMinPlayerCountTemplate<MainSceneNode>(param, filterParam);
	if (bestScene != entt::null) {
		return bestScene;
	}

	LOG_WARN << "No scene found with minimum player count";
	return FindSceneWithMinPlayerCountTemplate<MainSceneNode>(param, filterParam);
}

entt::entity NodeSceneSystem::FindNotFullScene(const GetSceneParams& param) {
	GetSceneFilterParam filterParam;

	auto bestScene = FindNotFullSceneTemplate<MainSceneNode>(param, filterParam);
	if (bestScene != entt::null) {
		return bestScene;
	}

	LOG_WARN << "No scene found that is not full";
	filterParam.nodePressureState = NodePressureState::kPressure;
	return FindNotFullSceneTemplate<MainSceneNode>(param, filterParam);
}

void NodeSceneSystem::SetNodePressure(entt::entity node) {
	auto* const nodeSceneComp = tls.GetNodeRegistry(eNodeType::SceneNodeService).try_get<NodeSceneComp>(node);

	if (nullptr == nodeSceneComp) {
		LOG_ERROR << "ServerComp not found for node: " << entt::to_integral(node);
		return;
	}

	nodeSceneComp->SetNodePressureState(NodePressureState::kPressure);
	LOG_INFO << "Node entered pressure state, Node ID: " << entt::to_integral(node);
}

void NodeSceneSystem::ClearNodePressure(entt::entity node) {
	auto* const nodeSceneComp = tls.GetNodeRegistry(eNodeType::SceneNodeService).try_get<NodeSceneComp>(node);

	if (nullptr == nodeSceneComp) {
		LOG_ERROR << "ServerComp not found for node: " << entt::to_integral(node);
		return;
	}

	nodeSceneComp->SetNodePressureState(NodePressureState::kNoPressure);
	LOG_INFO << "Node exited pressure state, Node ID: " << entt::to_integral(node);
}

void NodeSceneSystem::SetNodeState(entt::entity node, NodeState state) {
	auto* const tryServerComp = tls.GetNodeRegistry(eNodeType::SceneNodeService).try_get<NodeSceneComp>(node);

	if (nullptr == tryServerComp) {
		LOG_ERROR << "ServerComp not found for node: " << entt::to_integral(node);
		return;
	}

	tryServerComp->SetState(state);
	LOG_INFO << "Node state set successfully, Node ID: " << entt::to_integral(node) << ", State: " << static_cast<int>(state);
}
