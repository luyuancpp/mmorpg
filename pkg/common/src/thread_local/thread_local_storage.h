#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "entt/src/entt/signal/dispatcher.hpp"

using namespace entt;

class ThreadLocalStorage
{
public:
	entt::registry registry;
	entt::registry session_registry;
    entt::registry scene_registry;
    entt::registry player_registry;
    entt::registry game_node_registry;
    entt::registry gate_node_registry;
    entt::registry centre_node_registry;

	entt::dispatcher dispatcher;

	entt::entity op_entity{entt::null};
	entt::entity error_entity{entt::null};
	entt::entity global_entity{entt::null};

	void ClearForTest();
};

extern thread_local ThreadLocalStorage tls;