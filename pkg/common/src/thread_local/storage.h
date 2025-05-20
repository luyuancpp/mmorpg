#pragma once

#include "util/game_registry.h"
#include "util/snow_flake.h"

class ThreadLocalStorage
{
public:
	using RggistryMap = std::unordered_map<uint32_t, entt::registry>;
	entt::registry globalRegistry;
	entt::registry registry;
	entt::registry sceneRegistry;
	entt::registry sceneNodeRegistry;
	entt::registry gateNodeRegistry;
	entt::registry centreNodeRegistry;
	entt::registry itemRegistry;
	entt::registry sessionRegistry;
	entt::registry globalNodeRegistry;

	entt::dispatcher dispatcher;

	entt::entity operatorEntity{ entt::null };
	entt::entity errorEntity{ entt::null };
	entt::entity globalEntity{ entt::null };
	entt::entity globalGrpcNodeEntity{ entt::null };

	SnowFlake itemIdGenerator;
	Guid lastGeneratorItemGuid{ kInvalidGuid };

	void Clear();

	entt::registry& GetNodeRegistry(uint32_t type) {
		return nodeRegistries[type];
	}

	const RggistryMap& GetNodeRegistry() {
		return nodeRegistries;
	}


private:
	RggistryMap nodeRegistries;
};

extern thread_local ThreadLocalStorage tls;