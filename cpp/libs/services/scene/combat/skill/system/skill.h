#pragma once

#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class ReleaseSkillRequest;
class SkillTable;

class SkillSystem
{
public:
	static void StartCooldown(const entt::entity casterEntity, const SkillTable* skillTable);

	static uint32_t ReleaseSkill(entt::entity casterEntity, const ::ReleaseSkillRequest* request);
	static uint32_t CheckSkillPrerequisites(entt::entity casterEntity, const ::ReleaseSkillRequest* request);

	static bool IsSkillOfType(const uint32_t skillTableId, const uint32_t skillType);

	static void HandleGeneralSkillSpell(const entt::entity casterEntity, uint64_t skillId);
	static void HandleSkillRecovery(const entt::entity casterEntity, uint64_t skillId);
	static void HandleSkillFinish(const entt::entity casterEntity, uint64_t skillId);

	static void HandleChannelSkillSpell(entt::entity casterEntity, uint64_t skillId);
	static void HandleChannelThink(entt::entity casterEntity, uint64_t skillId);
	static void HandleChannelFinish(entt::entity casterEntity, uint64_t skillId);

    // Validate target
    static uint32_t ValidateTarget(const ::ReleaseSkillRequest* request);

    // Check cooldown
    static uint32_t CheckCooldown(entt::entity casterEntity, const SkillTable* skillTable);

    // Check cast timer
    static uint32_t CheckCasting(entt::entity casterEntity, const SkillTable* skillTable);

	static uint32_t CheckRecovery(const entt::entity casterEntity, const SkillTable* skillTable);

	static uint32_t CheckChannel(const entt::entity casterEntity, const SkillTable* skillTable);

    // Broadcast skill-used message
    static void BroadcastSkillUsedMessage(entt::entity casterEntity, const ::ReleaseSkillRequest* request);
	
    // Set up casting timer
    static void SetupCastingTimer(entt::entity casterEntity, const SkillTable* skillTable, uint64_t skillId);

    // Send skill-interrupted message
    static void SendSkillInterruptedMessage(entt::entity casterEntity, uint32_t skillTableId);

	static void TriggerSkillEffect(entt::entity casterEntity, uint64_t skillId);

	static void HandleSkillSpell(const entt::entity casterEntity, const uint64_t skillId);
};