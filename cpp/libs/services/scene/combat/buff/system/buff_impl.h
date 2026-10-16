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
    static void OnIntervalThink(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable, uint64_t nowMilliseconds);
    static void OnBuffStart(entt::entity parent, const BuffEntry& buff, const BuffTable* buffTable);
    static void OnBuffDestroy(entt::entity parent, uint64_t buffId, const BuffTable* buffTable);
    static void OnBeforeGiveDamage(entt::entity caster, entt::entity target, DamageEventComp& damageEvent);
    static void OnSkillHit(entt::entity caster, entt::entity target, uint64_t nowMilliseconds);

    // 受伤或被技能命中的目标移除加成，并从当前单调毫秒读数重新等待完整窗口。
    static void ResetCombatIdleBuff(entt::entity caster, entt::entity victim, uint64_t nowMilliseconds);

private:
    static void AddSilenceState(entt::entity parent, const BuffEntry& buff);
    static void RemoveSilenceState(entt::entity parent, uint64_t buffId);
    static void TickCombatIdleBuff(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable, uint64_t nowMilliseconds);
    static void ApplyNextBasicAttackBonus(entt::entity caster, entt::entity target, BuffEntry& buff,
                                          const BuffTable* buffTable, DamageEventComp& damageEvent,
                                          UInt64Set& buffsToRemove);
};

