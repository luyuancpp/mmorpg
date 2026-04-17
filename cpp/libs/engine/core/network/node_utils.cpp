#include "node_utils.h"

#include <thread_context/entity_manager.h>

NodeInfo &GetNodeInfo()
{
	return tlsEcs.globalRegistry.get_or_emplace<NodeInfo>(tlsEcs.GlobalEntity());
}

uint32_t GetZoneId()
{
	return GetNodeInfo().zone_id();
}
