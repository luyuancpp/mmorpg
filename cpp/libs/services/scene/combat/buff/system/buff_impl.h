#pragma once

#include "entt/src/entt/entity/entity.hpp"
#include "engine/core/type_define/type_define.h"
#include "combat/buff/comp/buff.h"

class BuffTable;
class DamageEventComp;

// BuffImplSystem: Handles buff lifecycle logic (type-specific behavior dispatch)
class BuffImplSystem {
public:
    static bool OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);
    static bool OnBuffStart(entt::entity parent, const BuffEntry& buffComp, const BuffTable* buffTable);
    static bool OnBuffDestroy(entt::entity parent, uint64_t buffId, const BuffTable* buffTable);
    static void OnBeforeGiveDamage(entt::entity casterEntity, entt::entity targetEntity, DamageEventComp& damageEvent);
    static void OnSkillHit(entt::entity casterEntity, entt::entity targetEntity);
    static void UpdateLastDamageOrSkillHitTime(entt::entity casterEntity, entt::entity targetEntity);

private:
    static bool HandleBuffStartSilence(entt::entity parent, const BuffEntry& buffComp);
    static bool HandleBuffDestroySilence(entt::entity parent, uint64_t buffId);
    static bool HandleIntervalNoDamageOrSkillHit(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);
    static void HandleBuffEffectsOnDamage(entt::entity casterEntity, entt::entity targetEntity, DamageEventComp& damageEvent);
    static void ApplyNextBasicAttackBuff(BuffEntry& buffComp, const BuffTable* buffTable, DamageEventComp& damageEvent, UInt64Set& buffsToRemoveCaster, entt::entity casterEntity, entt::entity targetEntity);
};

