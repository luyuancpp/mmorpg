#pragma once

#include "combat/buff/comp/buff.h"
#include "engine/core/type_define/type_define.h"

class BuffTable;
class SkillExecutedEvent;

// Caster (buff applier), Parent (buff target),
// Skill (which skill created this buff), BuffLayer (stacks),
// BuffLevel, BuffDuration,
// BuffTag, BuffImmuneTag,
// Context (contextual data at buff creation time).

class DamageEventComp;
struct BuffEntry;

class BuffSystem
{
public:
    static void Update(double delta);

    static std::tuple<uint32_t, uint64_t>  AddOrUpdateBuff(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext);

    static std::tuple<uint32_t, uint64_t> AddOrUpdateBuff(entt::entity parent, uint32_t buffTableId);

    static void RemoveBuff(entt::entity parent, uint64_t buffId);

    static void RemoveBuff(entt::entity parent, const UInt64Set& removeBuffIdList);

    static void RemoveSubBuff(BuffEntry& buffComp, UInt64Set& buffsToRemove);

    static void MarkBuffForRemoval(entt::entity parent, uint64_t buffId);

    // Remove pending buffs at end of frame
    static void RemovePendingBuffs(entt::entity parent, BuffListComp& buffListComp);

    // Check if buff can be created (checks target immunity)
    static uint32_t CanCreateBuff(entt::entity parentEntity, uint32_t buffTableId);

    static bool HandleExistingBuff(entt::entity parentEntity, uint32_t buffTableId, const SkillContextPtrComp& abilityContext);

    // Fired after instantiation but before activation (not yet in buff container).
    // Example: on receiving a debuff, dispel all debuffs and grant a shield.
    // A buff may be destroyed before it ever takes effect (watch lifecycle).
    static uint32_t OnBuffAwake(entt::entity parent, uint32_t buffTableId);

    // Called when buff takes effect (added to buff container). Designer-configurable.
    static void OnBuffStart(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);

    // Called when a same-type/same-caster buff exists: refresh stacks, level, duration.
    static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffEntry& buffComp);

    // Called before buff removal (still in buff container). Designer-configurable.
    static void OnBuffRemove(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);

    // Called after buff removal (already removed from container). Designer-configurable.
    static void OnBuffDestroy(entt::entity parent, const uint64_t buffId, const BuffTable* buffTable);

    // Interval-based periodic effect via StartIntervalThink. Designer-configurable.

    static void OnIntervalThink(entt::entity parent, uint64_t buffId);

    static void OnBuffExpire(entt::entity parent, uint64_t buffId);

    // Triggered when an active skill executes successfully
    static void OnSkillExecuted(SkillExecutedEvent& event);

    // Triggered when dealing damage to target
    static void OnBeforeGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent);

    // Triggered after dealing damage to target
    static void OnAfterGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent);

    // Triggered before taking damage
    static void OnBeforeTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent);

    // Triggered after taking damage
    static void OnAfterTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent);

    // Triggered before self dies
    static void OnBeforeDead(entt::entity parent);

    // Triggered after self dies
    static void OnAfterDead(entt::entity parent);

    // Triggered on killing a target
    static void OnKill(entt::entity parent);

    static void OnSkillHit(entt::entity casterEntity, entt::entity targetEntity);

    static bool AddSubBuffs(
        entt::entity parent,
        const BuffTable* buffTable,
        BuffEntry& buffComp
    );

    static void AddTargetSubBuffs(
        entt::entity targetEntity,
        const BuffTable* buffTable,
        const SkillContextPtrComp& abilityContext
    );

    static void AddSubBuffsWithoutCheck(
        entt::entity parent,
        const BuffTable* buffTable,
        BuffEntry& buffComp
    );
};


