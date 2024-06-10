#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "entt/src/entt/signal/dispatcher.hpp"

class ThreadLocalStorage
{
public:
	entt::registry registry;
	entt::registry session_registry;
    entt::registry scene_registry;
    entt::registry game_node_registry;
    entt::registry gate_node_registry;
    entt::registry centre_node_registry;
    entt::registry item_registry;
    entt::registry network_registry;
    entt::registry team_registry;

	entt::dispatcher dispatcher;

	entt::entity op_entity{entt::null};
	entt::entity error_entity{entt::null};
	entt::entity global_entity{entt::null};

	void ClearForTest();
};

void Destroy(entt::registry& registry, entt::entity entity);

extern thread_local ThreadLocalStorage tls;