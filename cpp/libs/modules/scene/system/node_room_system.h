#pragma once

#include "engine/core/node/constants/node_constants.h"
#include "engine/core/type_define/type_define.h"

constexpr uint32_t kDefaultSceneId = 1;

struct GetSceneParams
{
	uint32_t sceneConfigurationId{ kDefaultSceneId };
};

struct GetSceneFilterParam
{
	NodePressureState nodePressureState{ NodePressureState::kNoPressure };
};

class NodeSceneSystem
{
public:
	// Find a scene with the minimum player count among servers, less efficient
	static entt::entity FindSceneWithMinPlayerCount(const GetSceneParams& param);

	// Find a server with the scene where player count is not full, more efficient than the above function
	static entt::entity FindNotFullRoom(const GetSceneParams& param);

	// Set node pressure state
	static void SetNodePressure(entt::entity node);

	// Clear node pressure state
	static void ClearNodePressure(entt::entity node);

	// Set node state
	static void SetNodeState(entt::entity node, NodeState node_state);
};

