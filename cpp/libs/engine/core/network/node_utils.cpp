#include "node_utils.h"

#include "thread_context/registry_manager.h"
#include <thread_context/entity_manager.h>

NodeInfo* FindZoneUniqueNodeInfo(uint32_t zoneId, uint32_t nodeType) {
	auto& nodeRegistry = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
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
	return tlsEcs.globalRegistry.get_or_emplace<NodeInfo>(tlsEcs.GlobalEntity());
}

uint32_t GetZoneId()
{
	return GetNodeInfo().zone_id();
}

