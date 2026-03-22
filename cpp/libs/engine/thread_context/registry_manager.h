#pragma once

#include "core/utils/registry/game_registry.h"

#include <engine/core/type_define/type_define.h>


class RegistryManager
{
public:
	RegistryManager();

RegistryManager(const RegistryManager&) = delete;
    RegistryManager& operator=(const RegistryManager&) = delete;

	entt::registry globalRegistry;
	entt::registry actorRegistry;
	entt::registry sceneRegistry;
	entt::registry itemRegistry;
	entt::registry nodeGlobalRegistry;

	void Clear();

private:
};

extern thread_local RegistryManager tlsRegistryManager;