#pragma once

#include "util/game_registry.h"
#include "util/snow_flake.h"

#include <type_define/type_define.h>


class RegistryManager
{
public:

	RegistryManager();

	entt::registry globalRegistry;
	entt::registry actorRegistry;
	entt::registry sceneRegistry;
	entt::registry itemRegistry;
	entt::registry sessionRegistry;
	entt::registry nodeGlobalRegistry;

	void Clear();

private:
};

extern thread_local RegistryManager tlsManager;