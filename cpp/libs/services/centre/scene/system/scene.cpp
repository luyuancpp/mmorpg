#include "scene.h"
#include "muduo/base/Logging.h"

#include "modules/scene/comp/scene_comp.h"
#include "engine/core/node/constants/node_constants.h"

#include "table/proto/tip/scene_error_tip.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "proto/common/component/game_node_comp.pb.h"
#include "proto/common/event/scene_event.pb.h"
#include "proto/common/base/node.pb.h"
#include "engine/threading/node_context_manager.h"
#include "engine/threading/dispatcher_manager.h"

#include <ranges> // Only if using C++20 ranges
#include <threading/registry_manager.h>
#include <modules/scene/comp/scene_node_comp.h>
#include <modules/scene/system/scene_common.h>
#include "scene_node_selector.h"
#include "scene_selector.h"

// Constants
static constexpr std::size_t kMaxScenePlayer = 1000;


void AddMainSceneToNodeComponent(entt::registry& reg, const entt::entity node) {
	LOG_TRACE << "Adding main scene node components for entity: " << entt::to_integral(node);
	reg.emplace<MainSceneNode>(node);
	reg.emplace<SceneRegistryComp>(node);
	reg.emplace<SceneNodePlayerStatsPtrPbComponent>(node, std::make_shared<GameNodePlayerInfoPBComponent>());
}

// SceneUtil implementation
SceneSystem::SceneSystem() {
	LOG_TRACE << "SceneSystem constructor called";
	ClearAllSceneData();
}

SceneSystem::~SceneSystem() {
	LOG_TRACE << "SceneSystem destructor called";
	ClearAllSceneData();
}

void SceneSystem::ClearAllSceneData() {
	LOG_TRACE << "Clearing scene system data";
	tlsRegistryManager.sceneRegistry.clear();
	tlsRegistryManager.actorRegistry.clear();
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).clear();
}

// Get game node ID associated with a scene entity
NodeId SceneSystem::GetGameNodeIdFromSceneEntity(entt::entity scene) {
	auto* sceneInfo = tlsRegistryManager.sceneRegistry.try_get<SceneInfoPBComponent>(scene);
	if (sceneInfo) {
		return SceneCommon::GetGameNodeIdFromGuid(sceneInfo->guid());
	}
	else {
		LOG_ERROR << "SceneInfo not found for entity: " << entt::to_integral(scene);
		return kInvalidNodeId;
	}
}


// ✅ 2. 多节点负载均衡
// 你在 CompelPlayerChangeScene 中如果找不到房间就直接创建了：
//
// cpp
// 复制
// 编辑
// sceneEntity = CreateSceneOnSceneNode(p);
// 更好的做法是考虑负载均衡，比如：
//
// 查找资源空闲的节点（CPU / 玩家少）。
//
// 优先选择已有房间，再决定是否创建。

// Create a new scene associated with a game node

// Handle server node destruction
void SceneSystem::HandleDestroySceneNode(entt::entity node) {
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	auto& nodeSceneComp = registry.get<SceneRegistryComp>(node);
	auto sceneList = nodeSceneComp.GetSceneMap();

	// Destroy all scenes associated with the server node
	for (auto& confIdSceneList : sceneList | std::views::values) {
		for (auto scene : confIdSceneList) {
			SceneCommon::DestroyScene({ node, scene });
		}
	}

	// Destroy the server node itself
	DestroyEntity(registry, node);

	// Log server destruction
	LOG_INFO << "Destroyed server with ID: " << entt::to_integral(node);
}

// Enter a player into the default scene
void SceneSystem::EnterDefaultScene(const EnterDefaultSceneParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters when entering default scene";
		return;
	}

	// Get a scene that is not full from the NodeSceneSystem
	auto defaultScene = SceneNodeSelectorSystem::SelectAvailableScene({});

	// Enter the player into the retrieved default scene
	SceneCommon::EnterScene({ defaultScene, param.enter });

	// Log the entry into the default scene
	if (tlsRegistryManager.actorRegistry.any_of<Guid>(param.enter))
	{
		LOG_INFO << "Player entered default scene - Player GUID: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(param.enter) << ", Scene ID: " << entt::to_integral(defaultScene);
	}
}

// 这里只处理了同gs,如果是跨gs的scene切换，应该别的地方处理
void SceneSystem::CompelPlayerChangeScene(const CompelChangeSceneParam& param) {
	// ✅ 使用 FindOrCreateScene 替代原始杂糅逻辑
	entt::entity sceneEntity = SceneSystem::FindOrCreateScene(param.sceneConfId);

	SceneCommon::LeaveScene({ param.player });

	if (sceneEntity == entt::null) {
		EnterDefaultScene({ param.player });
		return;
	}

	SceneCommon::EnterScene({ sceneEntity, param.player });
}



// Replace a crashed server node with a new node
void SceneSystem::ReplaceCrashSceneNode(entt::entity crashNode, entt::entity destNode) {
	auto& sceneRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto& crashNodeScene = sceneRegistry.get<SceneRegistryComp>(crashNode);
	auto sceneList = crashNodeScene.GetSceneMap();

	for (auto& confIdSceneList : sceneList | std::views::values) {
		for (auto scene : confIdSceneList) {
			auto* sceneInfo = tlsRegistryManager.sceneRegistry.try_get<SceneInfoPBComponent>(scene);
			if (!sceneInfo) {
				continue;
			}
			CreateSceneOnNodeSceneParam p{ .node = destNode };
			p.sceneInfo.set_scene_confid(sceneInfo->scene_confid());
			SceneCommon::CreateSceneOnSceneNode(p);
		}
	}

	DestroyEntity(sceneRegistry, crashNode);
	LOG_INFO << "Replaced crashed server with new node: " << entt::to_integral(destNode);
}


entt::entity SceneSystem::FindOrCreateScene(uint32_t sceneConfId) {
	// 选择最优服务器节点
	entt::entity node = SelectBestNodeForScene(sceneConfId);
	if (node == entt::null) {
		LOG_ERROR << "FindOrCreateScene: Failed to select a scene node for sceneConfId = " << sceneConfId;
		return entt::null;
	}

	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto& nodeSceneComp = registry.get<SceneRegistryComp>(node);

	// 查找已有房间
	entt::entity scene = SceneSelectorSystem::SelectSceneWithMinPlayers(nodeSceneComp, sceneConfId);
	if (scene != entt::null) {
		return scene;
	}

	// 创建新房间
	CreateSceneOnNodeSceneParam createParam{ .node = node };
	createParam.sceneInfo.set_scene_confid(sceneConfId);

	scene = SceneCommon::CreateSceneOnSceneNode(createParam);
	if (scene == entt::null) {
		LOG_ERROR << "FindOrCreateScene: Failed to create scene for sceneConfId = " << sceneConfId;
	}

	return scene;
}

entt::entity SceneSystem::SelectBestNodeForScene(uint32_t sceneConfId) {
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	entt::entity bestNode = entt::null;
	std::size_t minPlayerCount = std::numeric_limits<std::size_t>::max();

	for (auto node : registry.view<SceneRegistryComp, SceneNodePlayerStatsPtrPbComponent>()) {
		const auto& nodeComp = registry.get<SceneRegistryComp>(node);
		const auto& playerInfoPtr = registry.get<SceneNodePlayerStatsPtrPbComponent>(node);
		if (!playerInfoPtr) continue;

		// 如果该节点已经有该配置的房间，优先考虑
		auto existingScenes = nodeComp.GetScenesByConfig(sceneConfId);
		if (!existingScenes.empty()) {
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
		LOG_WARN << "SelectBestNodeForScene: No suitable node found for sceneConfId = " << sceneConfId;
	}

	return bestNode;
}
