#include "room_node_state.h"
#include <core/node/constants/node_constants.h>
#include <muduo/base/Logging.h>
#include <threading/node_context_manager.h>
#include <modules/scene/comp/room_node_comp.h>

void RoomNodeStateSystem::MakeNodePressure(entt::entity node) {
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).emplace_or_replace<NodePressureComp>(node, NodePressureState::kPressure);
	LOG_INFO << "Node entered pressure state, Node ID: " << entt::to_integral(node);
}

void RoomNodeStateSystem::ClearNodePressure(entt::entity node) {
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).emplace_or_replace<NodePressureComp>(node, NodePressureState::kNoPressure);
	LOG_INFO << "Node exited pressure state, Node ID: " << entt::to_integral(node);
}

void RoomNodeStateSystem::MakeNodeState(entt::entity node, NodeState state) {
	tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).emplace_or_replace<NodeStateComp>(node, NodeState::kNormal);
	LOG_INFO << "Node state set successfully, Node ID: " << entt::to_integral(node) << ", State: " << static_cast<int>(state);
}
