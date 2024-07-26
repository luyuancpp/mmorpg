#pragma once

#include "util/game_registry.h"

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

	entt::entity op_entity{entt::null};
	entt::entity error_entity{entt::null};
	entt::entity global_entity{entt::null};

	void ClearForTest();
};

void Destroy(entt::registry& registry, entt::entity entity);

extern thread_local ThreadLocalStorage tls;