#pragma once

#include <cstdint>

#include <entt/src/entt/entity/entity.hpp>

class PlayerSkillSystem {
public:
	static void RegisterPlayer(entt::entity player);
	static bool HasSkill(entt::entity player, uint32_t skillTableId);
	static void SanitizeSkillList(entt::entity player);
};