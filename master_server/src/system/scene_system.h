#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/network/gs_node.h"

class SceneSystem
{
public:
	
};

template<typename ServerType>
static bool IsServerScene(entt::entity scene)
{
	auto try_scene_gs = registry.try_get<GsNodePtr>(scene);
	if (nullptr == try_scene_gs)
	{
		LOG_ERROR << " scene gs null : " << (nullptr == try_scene_gs);
		return false;
	}
	auto gs_it = g_gs_nodes.find((*try_scene_gs)->node_id());
	if (gs_it == g_gs_nodes.end())
	{
		LOG_ERROR << " scene gs null : " << (*try_scene_gs)->node_id();
		return false;
	}
	return registry.any_of<ServerType>(gs_it->second);
}
