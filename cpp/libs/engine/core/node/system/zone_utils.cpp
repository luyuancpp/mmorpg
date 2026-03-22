#include "zone_utils.h"
#include "node/system/node/node_util.h"
#include "proto/common/base/common.pb.h"

uint32_t GetZoneIdFromNodeId(NodeId nodeId) {
	entt::entity nodeEntity{ nodeId };

	auto& registry = NodeUtils::GetRegistryForNodeType(eNodeType::SceneNodeService);

	const NodeInfo* nodeInfo = registry.try_get<NodeInfo>(nodeEntity);
	if (!nodeInfo) {
		return kInvalidNodeId;
	}

	return nodeInfo->zone_id();
}


bool IsCrossZone(NodeId  fromNodeId, NodeId toNodeId) {
	return GetZoneIdFromNodeId(fromNodeId) != GetZoneIdFromNodeId(toNodeId);
}