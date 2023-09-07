#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "entt/src/entt/signal/dispatcher.hpp"

struct ThreadLocalStorage
{
	entt::registry registry;
	entt::dispatcher dispatcher;
	entt::entity op_entity{entt::null};
	entt::entity error_entity{entt::null};
	entt::entity global_entity{entt::null};
};

extern thread_local ThreadLocalStorage tls;