#pragma once

#include "entt/src/entt/entity/entity.hpp"
#include "engine/core/node/constants/node_constants.h"

class RoomNodeStateSystem
{
public:

	// Set node pressure state
	static void MakeNodePressure(entt::entity node);

	// Clear node pressure state
	static void ClearNodePressure(entt::entity node);

	// Set node state
	static void MakeNodeState(entt::entity node, NodeState node_state);
};

