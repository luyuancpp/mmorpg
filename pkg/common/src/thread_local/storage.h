#pragma once

#include "util/game_registry.h"
#include "util/snow_flake.h"

constexpr uint32_t kMaxNodeType = 20;

class ThreadLocalStorage
{
public:
	using Rggistries = std::array<entt::registry, kMaxNodeType>;
	entt::registry globalRegistry;
	entt::registry registry;
	entt::registry sceneRegistry;
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

	const Rggistries& GetNodeRegistry() {
		return nodeRegistries;
	}


private:
	Rggistries nodeRegistries;
};

extern thread_local ThreadLocalStorage tls;