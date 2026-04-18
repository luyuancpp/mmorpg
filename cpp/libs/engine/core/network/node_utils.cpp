#include "node_utils.h"

#include <thread_context/entity_manager.h>
#include <engine/thread_context/node_context_manager.h>

NodeInfo &GetNodeInfo()
{
	return tlsEcs.globalRegistry.get_or_emplace<NodeInfo>(tlsEcs.GlobalEntity());
}

uint32_t GetZoneId()
{
	return GetNodeInfo().zone_id();
}

entt::entity GetSceneManagerEntity(Guid playerId)
{
	auto &smRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneManagerNodeService);
	auto view = smRegistry.view<NodeInfo>();
	const auto count = view.size();
	if (count == 0)
	{
		return entt::null;
	}
	const auto index = playerId % count;
	size_t i = 0;
	for (const auto entity : view)
	{
		if (i == index)
		{
			return entity;
		}
		++i;
	}
	return entt::null;
}
