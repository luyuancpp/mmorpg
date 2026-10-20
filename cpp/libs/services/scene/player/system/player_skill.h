#pragma once

#include <cstdint>

#include <entt/src/entt/entity/entity.hpp>

class PlayerSkillSystem {
public:
	static void RegisterPlayer(entt::entity player);
	// 玩家拥有且本版本生效(IsActiveSkill)的技能才算"有"。
	static bool HasSkill(entt::entity player, uint32_t skillTableId);
	// 去掉 0 与重复项;本版本技能表查不到的技能**保留但不生效**,存盘原样写回(混版本设计 §12,第 5 批)。
	static void SanitizeSkillList(entt::entity player);
	// 技能列表里的这一项在本版本是否生效:非 0 且技能表查得到。消费 PlayerSkillListComp 的路径
	// (施放、下发客户端、战斗快照……)一律只认生效的项 —— 列表里可能留着新版本技能表才有的技能。
	static bool IsActiveSkill(uint32_t skillTableId);
};