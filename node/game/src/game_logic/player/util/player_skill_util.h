#pragma once

#include "util/game_registry.h"

class PlayerSkillUtil {
public:
	static void InitializePlayerComponentsHandler(entt::entity player);
	static void RegisterPlayer(entt::entity player);
};