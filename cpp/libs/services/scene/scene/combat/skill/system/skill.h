#pragma once

#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>


#include "proto/scene/player_skill.pb.h"
//https://zhuanlan.zhihu.com/p/149704315

class SkillTable;

class SkillSystem
{
public:
	static void StartCooldown(const entt::entity casterEntity, const SkillTable* skillTable);

	static uint32_t ReleaseSkill(entt::entity casterEntity, const ::ReleaseSkillSkillRequest* request);
	static uint32_t CheckSkillPrerequisites(entt::entity casterEntity, const ::ReleaseSkillSkillRequest* request);

	static bool IsSkillOfType(const uint32_t skillTableId, const uint32_t skillType);

	static void HandleSkillInitialize();
	static void HandleGeneralSkillSpell(const entt::entity casterEntity, uint64_t skillId);
	static void HandleSkillRecovery(const entt::entity casterEntity, uint64_t skillId);
	static void HandleSkillFinish(const entt::entity casterEntity, uint64_t skillId);

	static void HandleChannelSkillSpell(entt::entity casterEntity, uint64_t skillId);
	static void HandleChannelThink(entt::entity casterEntity, uint64_t skillId);
	static void HandleChannelFinish(entt::entity casterEntity, uint64_t skillId);

	static void HandleSkillToggleOn(entt::entity casterEntity, uint64_t skillId);
	static void HandleSkillToggleOff(entt::entity casterEntity, uint64_t skillId);

	static void HandleSkillActivate(entt::entity casterEntity, uint64_t skillId);
	static void HandleSkillDeactivate(entt::entity casterEntity, uint64_t skillId);

    // Validate target
    static uint32_t ValidateTarget(const ::ReleaseSkillSkillRequest* request);

    // Check cooldown
    static uint32_t CheckCooldown(entt::entity casterEntity, const SkillTable* skillTable);

    // Check cast timer
    static uint32_t CheckCasting(entt::entity casterEntity, const SkillTable* skillTable);

	static uint32_t CheckRecovery(const entt::entity casterEntity, const SkillTable* skillTable);

	static uint32_t CheckChannel(const entt::entity casterEntity, const SkillTable* skillTable);

    // Broadcast skill-used message
    static void BroadcastSkillUsedMessage(entt::entity casterEntity, const ::ReleaseSkillSkillRequest* request);
	
    // Set up casting timer
    static void SetupCastingTimer(entt::entity casterEntity, const SkillTable* skillTable, uint64_t skillId);

    // Send skill-interrupted message
    static void SendSkillInterruptedMessage(entt::entity casterEntity, const uint32_t skillTableId) ;

	static void TriggerSkillEffect(entt::entity casterEntity, uint64_t skillId);

	static void RemoveEffect(entt::entity casterEntity, uint64_t skillId);

	static void HandleSkillSpell(const entt::entity casterEntity, const uint64_t skillId);
};