#pragma once

#include "entt/src/entt/entity/entity.hpp"
#include "engine/core/type_define/type_define.h"
#include "combat/buff/comp/buff_comp.h"

class BuffTable;
class DamageEventComp;

// BuffImplSystem: type-specific buff behaviour
// (silence state / combat-idle bonus / next-basic-attack bonus).
class BuffImplSystem {
public:
    static void OnIntervalThink(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);
    static void OnBuffStart(entt::entity parent, const BuffEntry& buff, const BuffTable* buffTable);
    static void OnBuffDestroy(entt::entity parent, uint64_t buffId, const BuffTable* buffTable);
    static void OnBeforeGiveDamage(entt::entity caster, entt::entity target, DamageEventComp& damageEvent);
    static void OnSkillHit(entt::entity caster, entt::entity target);

    // Reset the "no damage/skill hit in last N seconds" buff on the victim that
    // just took damage or was hit by a skill (re-enters combat).
    static void ResetCombatIdleBuff(entt::entity caster, entt::entity victim);

private:
    static void AddSilenceState(entt::entity parent, const BuffEntry& buff);
    static void RemoveSilenceState(entt::entity parent, uint64_t buffId);
    static void TickCombatIdleBuff(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);
    static void ApplyNextBasicAttackBonus(entt::entity caster, entt::entity target, BuffEntry& buff,
                                          const BuffTable* buffTable, DamageEventComp& damageEvent,
                                          UInt64Set& buffsToRemove);
};

