#include "zone_utils.h"
#include "node/system/node/node_util.h"
#include "proto/common/common.pb.h"

uint32_t GetZoneIdFromNodeId(NodeId nodeId) {
	entt::entity nodeEntity{ nodeId };

	// 获取 SceneNodeService 类型的 registry
	auto& registry = NodeUtils::GetRegistryForNodeType(eNodeType::RoomNodeService);

	// 尝试获取该节点的 NodeInfo 组件
	const NodeInfo* nodeInfo = registry.try_get<NodeInfo>(nodeEntity);
	if (!nodeInfo) {
		return kInvalidNodeId;
	}

	return nodeInfo->zone_id();
}


bool IsCrossZone(NodeId  fromNodeId, NodeId toNodeId) {
	return GetZoneIdFromNodeId(fromNodeId) != GetZoneIdFromNodeId(toNodeId);
}