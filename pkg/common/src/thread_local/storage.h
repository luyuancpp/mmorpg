#pragma once

#include "util/game_registry.h"
#include "util/snow_flake.h"

class ThreadLocalStorage
{
public:
    entt::registry globalRegistry;
	entt::registry registry;
    entt::registry sceneRegistry;
    entt::registry gameNodeRegistry;
    entt::registry gateNodeRegistry;
    entt::registry centreNodeRegistry;
    entt::registry itemRegistry;
    entt::registry networkRegistry;

	entt::dispatcher dispatcher;

	entt::entity operatorEntity{entt::null};
	entt::entity errorEntity{entt::null};
	entt::entity globalEntity{entt::null};

    SnowFlake itemIdGenerator;

	void ClearForTest();
};

void Destroy(entt::registry& registry, entt::entity entity);

extern thread_local ThreadLocalStorage tls;