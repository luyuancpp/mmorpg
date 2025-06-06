#pragma once

#include "util/game_registry.h"
#include "util/snow_flake.h"

constexpr uint32_t kMaxNodeType = 20;

class ThreadLocalStorage
{
public:
	using NodeRgistries = std::array<entt::registry, kMaxNodeType>;
	using NodeGloabalEntity = std::array<entt::entity, kMaxNodeType>;

	ThreadLocalStorage();

	entt::registry globalRegistry;
	entt::registry registry;
	entt::registry sceneRegistry;
	entt::registry itemRegistry;
	entt::registry sessionRegistry;
	entt::registry nodeGlobalRegistry;

	entt::dispatcher dispatcher;

	entt::entity operatorEntity{ entt::null };
	entt::entity errorEntity{ entt::null };
	entt::entity globalEntity{ entt::null };
	entt::entity globalGrpcNodeEntity{ entt::null };

	SnowFlake itemIdGenerator;
	Guid lastGeneratorItemGuid{ kInvalidGuid };

	void Clear();

	entt::registry& GetNodeRegistry(uint32_t nodeType) {
		return nodeRegistries[nodeType];
	}

	NodeRgistries& GetNodeRegistry() {
		return nodeRegistries;
	}

	entt::entity GetNodeGlobalEntity(uint32_t nodeType);
private:
	NodeRgistries nodeRegistries;
	NodeGloabalEntity nodeGlobalEntities;
};

extern thread_local ThreadLocalStorage tls;