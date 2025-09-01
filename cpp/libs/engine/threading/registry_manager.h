#pragma once

#include "core/utils/registry/game_registry.h"

#include <base/core/type_define/type_define.h>


class RegistryManager
{
public:
	RegistryManager();

    // 禁止拷贝和移动，确保单例唯一性
    RegistryManager(const RegistryManager&) = delete;
    RegistryManager& operator=(const RegistryManager&) = delete;

	entt::registry globalRegistry;
	entt::registry actorRegistry;
	entt::registry sceneRegistry;
	entt::registry itemRegistry;
	entt::registry sessionRegistry;
	entt::registry nodeGlobalRegistry;

	void Clear();

private:
};

extern thread_local RegistryManager tlsRegistryManager;