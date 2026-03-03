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

void SceneSystem::CompelPlayerChangeScene(const CompelChangeSceneParam& param) {
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
	GetSceneParams params{ sceneConfId };

	// 1. Try to find an existing scene with available slots
	entt::entity scene = SceneNodeSelectorSystem::SelectAvailableScene(params);
	if (scene != entt::null) {
		return scene;
	}

	// 2. If no available scene found, select the best node to create a new one
	entt::entity node = SceneNodeSelectorSystem::SelectBestNodeForCreation(params);
	if (node == entt::null) {
		LOG_ERROR << "FindOrCreateScene: Failed to find a suitable node to create scene " << sceneConfId;
		return entt::null;
	}

	// 3. Create the scene on the selected node
	CreateSceneOnNodeSceneParam createParam{ .node = node };
	createParam.sceneInfo.set_scene_confid(sceneConfId);

	scene = SceneCommon::CreateSceneOnSceneNode(createParam);
	if (scene == entt::null) {
		LOG_ERROR << "FindOrCreateScene: Failed to create scene " << sceneConfId << " on node " << entt::to_integral(node);
	}

	return scene;
}
