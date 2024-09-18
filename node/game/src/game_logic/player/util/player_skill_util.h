#pragma once

#include "util/game_registry.h"

class PlayerSkillUtil {
public:
	static void InitializeAfterDBLoad(entt::entity player);
	static void RegisterPlayer(entt::entity player);
};