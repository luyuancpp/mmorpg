#include "node_utils.h"

#include "threading/registry_manager.h"
#include <threading/entity_manager.h>

NodeInfo* FindZoneUniqueNodeInfo(uint32_t zoneId, uint32_t nodeType) {
	auto& nodeRegistry = tlsRegistryManager.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& nodeList = *nodeRegistry[nodeType].mutable_node_list();
	for (auto& node : nodeList)
	{
		if (node.zone_id() == zoneId)
		{
			return &node;
		}
	}
	return nullptr;
}

NodeInfo& GetNodeInfo()
{
	return tlsRegistryManager.globalRegistry.get_or_emplace<NodeInfo>(GlobalEntity());
}

uint32_t GetZoneId()
{
	return GetNodeInfo().zone_id();
}
