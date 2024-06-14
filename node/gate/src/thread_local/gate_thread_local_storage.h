#pragma once

#include <memory>

#include "util/game_registry.h"

class GateThreadLocalStorage
{
public:
	GateThreadLocalStorage();
    entt::registry login_node_registry;

private:
};

extern thread_local GateThreadLocalStorage gate_tls;

