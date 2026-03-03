#include "scene_node_selector.h"
#include <ranges>
#include "modules/scene/comp/scene_node_comp.h"

#include "proto/common/component/game_node_comp.pb.h"
#include "proto/common/base/node.pb.h"
#include "muduo/base/Logging.h"
#include "threading/node_context_manager.h"
#include "scene_selector.h"

// 常量定义
namespace {
	constexpr std::size_t kInvalidPlayerCount = std::numeric_limits<std::size_t>::max();

	// 选择匹配的节点
	template <typename ServerType>
	entt::entity FindBestNode(uint32_t configId, NodePressureState desiredPressure, bool preferLeastLoaded) {
		entt::entity bestNode{ entt::null };
		std::size_t bestLoad = kInvalidPlayerCount;

		auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

		for (auto entity : nodeRegistry.view<ServerType>()) {
			const auto& sceneRegistry = nodeRegistry.get<SceneRegistryComp>(entity);

			// 判断节点是否有效
			if (!nodeRegistry.get_or_emplace<NodeStateComp>(entity, NodeState::kNormal).IsNormal() ||
				sceneRegistry.GetScenesByConfig(configId).empty() ||
				nodeRegistry.get_or_emplace<NodePressureComp>(entity, NodePressureState::kNoPressure) != desiredPressure) {
				continue;
			}

			std::size_t playerCount = nodeRegistry.get<SceneNodePlayerStatsPtrPbComponent>(entity)->player_size();

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
			LOG_WARN << "No suitable node found for scene config ID: " << configId;
		}

		return bestNode;
	}

	// 在节点中选择玩家最少的房间
	entt::entity FindSceneWithMinPlayers(entt::entity nodeEntity, uint32_t configId) {
		auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
		const auto& sceneRegistry = nodeRegistry.get<SceneRegistryComp>(nodeEntity);

		auto scene = SceneSelectorSystem::SelectSceneWithMinPlayers(sceneRegistry, configId);

		if (scene == entt::null) {
			LOG_WARN << "No suitable scene found with minimum players on node: " << entt::to_integral(nodeEntity);
		}

		return scene;
	}

	// 在节点中选择一个未满的房间
	entt::entity FindFirstAvailableScene(entt::entity nodeEntity, uint32_t configId) {
		auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
		const auto& sceneRegistry = nodeRegistry.get<SceneRegistryComp>(nodeEntity);

		for (const auto& scene : sceneRegistry.GetScenesByConfig(configId)) {
			std::size_t scenePlayerCount = tlsRegistryManager.sceneRegistry.get<ScenePlayers>(scene).size();
			if (scenePlayerCount < kMaxPlayersPerScene) {
				return scene;
			}
		}

		LOG_WARN << "No available (not full) scene found on node: " << entt::to_integral(nodeEntity);
		return entt::null;
	}
}

// 外部接口：选择负载最小的房间
entt::entity SceneNodeSelectorSystem::SelectLeastLoadedScene(const GetSceneParams& param) {
	constexpr GetSceneFilterParam defaultFilter;

	auto node = FindBestNode<MainSceneNode>(param.sceneConfigurationId, defaultFilter.nodePressureState, true);
	if (node == entt::null) return entt::null;

	return FindSceneWithMinPlayers(node, param.sceneConfigurationId);
}

// 外部接口：选择第一个可用房间
entt::entity SceneNodeSelectorSystem::SelectAvailableScene(const GetSceneParams& param) {
	GetSceneFilterParam filter;

	auto node = FindBestNode<MainSceneNode>(param.sceneConfigurationId, filter.nodePressureState, false);
	if (node != entt::null) {
		auto scene = FindFirstAvailableScene(node, param.sceneConfigurationId);
		if (scene != entt::null) return scene;
	}

	// 尝试在高压力节点中再查一次
	filter.nodePressureState = NodePressureState::kPressure;

	node = FindBestNode<MainSceneNode>(param.sceneConfigurationId, filter.nodePressureState, false);
	if (node != entt::null) {
		return FindFirstAvailableScene(node, param.sceneConfigurationId);
	}

	LOG_WARN << "No available scene found even under pressure for config ID: " << param.sceneConfigurationId;
	return entt::null;
}

entt::entity SceneNodeSelectorSystem::SelectBestNodeForCreation(const GetSceneParams& param) {
	entt::entity bestNode{ entt::null };
	std::size_t bestLoad = kInvalidPlayerCount;

	auto& nodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	for (auto entity : nodeRegistry.view<MainSceneNode>()) {
		// Check if node is normal
		if (!nodeRegistry.get_or_emplace<NodeStateComp>(entity, NodeState::kNormal).IsNormal()) {
			continue;
		}

		// Check player count
		std::size_t playerCount = 0;
		if (auto* stats = nodeRegistry.try_get<SceneNodePlayerStatsPtrPbComponent>(entity)) {
			playerCount = (*stats)->player_size();
		}

		if (playerCount < bestLoad && playerCount < kMaxServerPlayerSize) {
			bestNode = entity;
			bestLoad = playerCount;
		}
	}

	if (bestNode == entt::null) {
		LOG_ERROR << "SelectBestNodeForCreation: No suitable node found to create scene " << param.sceneConfigurationId;
	}

	return bestNode;
}
