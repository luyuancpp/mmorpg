#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "entt/src/entt/signal/dispatcher.hpp"


struct ThreadLocalStorage
{
	entt::registry registry;
	entt::dispatcher dispatcher;

};

extern thread_local ThreadLocalStorage tls;