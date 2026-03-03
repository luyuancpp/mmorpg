#pragma once
#include <cstdint>

#include "entt/src/entt/entity/entity.hpp"

class SceneRegistryComp;

class SceneSelectorSystem {
public:
	static entt::entity SelectSceneWithMinPlayers(const SceneRegistryComp& comp, uint32_t configId);
};
