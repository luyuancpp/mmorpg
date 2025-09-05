#include "room_node_state.h"
#include <core/node/constants/node_constants.h>
#include <muduo/base/Logging.h>
#include <threading/node_context_manager.h>
#include <modules/scene/comp/room_node_comp.h>

void RoomNodeStateSystem::MakeNodePressure(entt::entity node) {
	auto* const nodeSceneComp = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<NodeRoomComp>(node);

	if (nullptr == nodeSceneComp) {
		LOG_ERROR << "ServerComp not found for node: " << entt::to_integral(node);
		return;
	}

	nodeSceneComp->SetNodePressureState(NodePressureState::kPressure);
	LOG_INFO << "Node entered pressure state, Node ID: " << entt::to_integral(node);
}

void RoomNodeStateSystem::ClearNodePressure(entt::entity node) {
	auto* const nodeSceneComp = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<NodeRoomComp>(node);

	if (nullptr == nodeSceneComp) {
		LOG_ERROR << "ServerComp not found for node: " << entt::to_integral(node);
		return;
	}

	nodeSceneComp->SetNodePressureState(NodePressureState::kNoPressure);
	LOG_INFO << "Node exited pressure state, Node ID: " << entt::to_integral(node);
}

void RoomNodeStateSystem::MakeNodeState(entt::entity node, NodeState state) {
	auto* const tryServerComp = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).try_get<NodeRoomComp>(node);

	if (nullptr == tryServerComp) {
		LOG_ERROR << "ServerComp not found for node: " << entt::to_integral(node);
		return;
	}

	tryServerComp->SetState(state);
	LOG_INFO << "Node state set successfully, Node ID: " << entt::to_integral(node) << ", State: " << static_cast<int>(state);
}
