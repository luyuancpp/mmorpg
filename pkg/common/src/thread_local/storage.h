#pragma once

#include "util/game_registry.h"
#include "util/snow_flake.h"

class ThreadLocalStorage
{
public:
	entt::registry globalRegistry;
	entt::registry registry;
	entt::registry sceneRegistry;
	entt::registry sceneNodeRegistry;
	entt::registry gateNodeRegistry;
	entt::registry centreNodeRegistry;
	entt::registry itemRegistry;
	entt::registry networkRegistry;
	entt::registry globalNodeRegistry;
	entt::registry invalidRegistry;

	entt::dispatcher dispatcher;

	entt::entity operatorEntity{ entt::null };
	entt::entity errorEntity{ entt::null };
	entt::entity globalEntity{ entt::null };
	entt::entity globalGrpcNodeEntity{ entt::null };

	SnowFlake itemIdGenerator;
	Guid lastGeneratorItemGuid{ kInvalidGuid };

	void Clear();
	std::string GetRegistryName(const entt::registry& registry) const;
};

extern thread_local ThreadLocalStorage tls;