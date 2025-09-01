#pragma once

#include "core/utils/registry/game_registry.h"

class PlayerSkillSystem {
public:
	static void InitializePlayerComponentsHandler(entt::entity player);
	static void RegisterPlayer(entt::entity player);
};