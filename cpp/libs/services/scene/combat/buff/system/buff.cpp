#include "buff.h"
#include <ranges>
#include "table/code/buff_table.h"
#include "combat/buff/system/buff_impl.h"
#include "proto/common/component/buff_comp.pb.h"
#include <muduo/base/Logging.h>
#include "table/proto/tip/buff_error_tip.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "macros/error_return.h"
#include "modifier_buff_impl.h"
#include "motion_modifier_impl.h"
#include "combat/buff/comp/buff_comp.h"
#include "combat/buff/constants/buff.h"
#include "proto/common/event/skill_event.pb.h"
#include "core/utils/utility/utility.h"
#include "player/comp/player_frozen_comp.h" // Frozen exclude — cross-zone-readiness-audit.md §11.2
#include "core/system/id_generator.h"
#include <thread_context/ecs_context.h>


// TODO: Combat logic must run on frames, not timers. This ensures buff triggers and expirations
// happen within frame logic, avoiding issues where timer callbacks fire after entity
// destruction or buff removal. Consider migrating all buff expiry and periodic triggers
// to per-frame processing.

uint64_t GenerateUniqueBuffId(const BuffListComp& buffList)
{
    uint64_t newBuffId = UINT64_MAX;
    do {
        newBuffId = tlsIdGeneratorManager.buffIdGenerator.Generate();
    } while (buffList.contains(newBuffId) || newBuffId == UINT64_MAX);
    return newBuffId;
}

bool IsTargetImmune(const BuffListComp& buffList, const BuffTable* buffTableParam)
{
    for (const auto& buff : buffList | std::views::values) {
        LookupBuffOrReturnError(buff.buffPb.buff_table_id());
        for (const auto& tag : buffTableParam->tag() | std::views::keys) {
            if (buffRow->immune_tag().contains(tag)) {
                return true;
            }
        }
    }
    return false;
}

BuffMessagePtr CreateBuffDataPtr(const BuffTable* buffTable) {
    switch (buffTable->buff_type()) {
    case kBuffTypeNoDamageOrSkillHitInLastSeconds:
        return std::make_shared<BuffNoDamageOrSkillHitInLastSecondsComp>();
    default:
        return nullptr;
    }
}

// True if the entity still has a stealth buff other than the given one.
bool HasStealthBuffExcept(const entt::entity parent, const uint64_t excludeBuffId)
{
    const auto* buffList = tlsEcs.actorRegistry.try_get<BuffListComp>(parent);
    if (!buffList) {
        return false;
    }

    for (const auto& entry : *buffList | std::views::values) {
        if (entry.buffPb.buff_id() == excludeBuffId) {
            continue;
        }
        const auto [tbl, res] = BuffTableManager::Instance().FindById(entry.buffPb.buff_table_id());
        if (tbl && tbl->buff_type() == kBuffTypeStealth) {
            return true;
        }
    }
    return false;
}

// Add or update buff (with abilityContext)
std::tuple<uint32_t, uint64_t> BuffSystem::AddOrUpdateBuff(
    const entt::entity parent,
    const uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext)
{
    if (!tlsEcs.actorRegistry.valid(parent))
    {
        return {kThisEntityIsInvalid, UINT64_MAX};
    }

    LookupBuffOrReturn(buffTableId, (std::make_tuple(buffResult, UINT64_MAX)));

    if (const auto result = CanCreateBuff(parent, buffTableId); result != kSuccess) {
        return {result, UINT64_MAX};
    }

    auto& buffList = tlsEcs.actorRegistry.get_or_emplace<BuffListComp>(parent);

    // Pure dispel buffs and stack/refresh of an existing buff don't create a new entry.
    if (DispelBuffsOnAwake(parent, buffTableId)) {
        return {kSuccess, UINT64_MAX};
    }

    if (HandleExistingBuff(parent, buffTableId, abilityContext)) {
        return {kSuccess, UINT64_MAX};
    }

    BuffEntry newBuff;
    if (abilityContext != nullptr)
    {
        newBuff.buffPb.set_caster(abilityContext->caster());
    }
    newBuff.buffPb.set_processed_caster(buffRow->no_caster() ? entt::null : (abilityContext ? abilityContext->caster() : entt::null));

    uint64_t newBuffId = GenerateUniqueBuffId(buffList);
    newBuff.buffPb.set_buff_id(newBuffId);
    newBuff.buffPb.set_buff_table_id(buffTableId);
    newBuff.skillContext = abilityContext;
    newBuff.dataPbPtr = CreateBuffDataPtr(buffRow);

    auto [fst, snd] = buffList.emplace(newBuffId, std::move(newBuff));
    OnBuffStart(parent, fst->second, buffRow);

    if (buffRow->duration() > 0) {
        fst->second.expireTimerTaskComp.RunAfter(buffRow->duration(), [parent, newBuffId] {
            if (!tlsEcs.actorRegistry.valid(parent))
            {
                return;
            }
            OnBuffExpire(parent, newBuffId);
            });
    }
    else if (IsZero(buffRow->duration())) {
        OnBuffExpire(parent, newBuffId);
    }

    return {kSuccess, newBuffId};
}

// Add or update buff (without abilityContext)
std::tuple<uint32_t, uint64_t> BuffSystem::AddOrUpdateBuff(
    const entt::entity parent,
    const uint32_t buffTableId)
{
    return AddOrUpdateBuff(parent, buffTableId, nullptr);
}


// Remove buff
void BuffSystem::RemoveBuff(const entt::entity parent, const uint64_t buffId)
{
    OnBuffExpire(parent, buffId);
}

void BuffSystem::RemoveBuff(const entt::entity parent, const UInt64Set& removeBuffIdList) {
    for (auto& removeBuffId : removeBuffIdList) {
        BuffSystem::RemoveBuff(parent, removeBuffId);
    }
}

void BuffSystem::RemoveSubBuff(BuffEntry& buffComp, UInt64Set& buffsToRemove)
{
    for (auto& [subBuffId, _] : buffComp.buffPb.sub_buff_list_id())
    {
        buffsToRemove.emplace(subBuffId);
    }

    buffComp.buffPb.clear_sub_buff_list_id();
}

void BuffSystem::MarkBuffForRemoval(const entt::entity parent, uint64_t buffId) {
    auto& pendingRemoveBuffs = tlsEcs.actorRegistry.get_or_emplace<BuffPendingRemoveBuffs>(parent);
    pendingRemoveBuffs.emplace(buffId);
}

// Remove pending buffs at end of frame
void BuffSystem::RemovePendingBuffs(const entt::entity parent, BuffListComp& buffListComp) {
    auto& pendingRemoveBuffs = tlsEcs.actorRegistry.get_or_emplace<BuffPendingRemoveBuffs>(parent);

    for (const auto& buffId : pendingRemoveBuffs) {
        buffListComp.erase(buffId);
        LOG_TRACE << "Buff with ID " << buffId << " removed from entity at end of frame.\n";
    }

    pendingRemoveBuffs.clear();
}

// Buff expiry handler
void BuffSystem::OnBuffExpire(const entt::entity parent, const uint64_t buffId)
{
    auto *buffListPtr = tlsEcs.actorRegistry.try_get<BuffListComp>(parent);
    if (!buffListPtr)
    {
        LOG_ERROR << "Cannot find buff list for entity " << entt::to_integral(parent);
        return;
    }
    auto &buffList = *buffListPtr;
    const auto buffIt = buffList.find(buffId);

    if (buffIt == buffList.end()) {
        LOG_ERROR << "Cannot find buff " << buffId;
        return;
    }

    const auto buffTableId = buffIt->second.buffPb.buff_table_id();
    LookupBuffOrReturnVoid(buffTableId);

    OnBuffRemove(parent, buffIt->second, buffRow);
    buffList.erase(buffId);
    OnBuffDestroy(parent, buffId, buffRow);
}

// Check if buff can be created
uint32_t BuffSystem::CanCreateBuff(const entt::entity parentEntity, const uint32_t buffTableId)
{
    LookupBuffOrReturnError(buffTableId);

    const auto *buffList = tlsEcs.actorRegistry.try_get<BuffListComp>(parentEntity);
    if (buffList)
    {
        if (const bool isImmune = IsTargetImmune(*buffList, buffRow))
        {
            return MAKE_ERROR_MSG(kBuffTargetImmuneToBuff,
                                  "entity=" << entt::to_integral(parentEntity)
                                            << " buffTableId=" << buffTableId);
        }
    }

    return kSuccess;
}

// Handle existing buff (stack/refresh)
bool BuffSystem::HandleExistingBuff(const entt::entity parentEntity,
    const uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext)
{
    LookupBuffOrReturnFalse(buffTableId);

    if (!abilityContext) {
        return false;
    }

    auto& buffList = tlsEcs.actorRegistry.get_or_emplace<BuffListComp>(parentEntity);
    for (auto& buffComp : buffList | std::views::values) {
        if (buffComp.buffPb.buff_table_id() == buffTableId && buffComp.buffPb.processed_caster() == abilityContext->caster()) {
            if (buffComp.buffPb.layer() < buffRow->max_layer()) {
                buffComp.buffPb.set_layer(buffComp.buffPb.layer() + 1);
            }
            OnBuffRefresh(parentEntity, buffTableId, abilityContext, buffComp);
            return true;
        }
    }
    return false;
}

// Dispel matching buffs on awake; returns true if this is a pure Dispel buff
// (consumed without being added to the buff list).
bool BuffSystem::DispelBuffsOnAwake(const entt::entity parent, const uint32_t buffTableId)
{
    const auto [addBuffRow, addBuffResult] = BuffTableManager::Instance().FindByIdSilent(buffTableId);
    if (!addBuffRow) {
        LOG_ERROR << "Buff row not found for ID: " << buffTableId;
        return false;
    }

    UInt64Vector dispelBuffIdList;
    auto &buffList = tlsEcs.actorRegistry.get<BuffListComp>(parent);
    for (auto& [buffId, entry] : buffList) {
        LookupBuffOrContinue(entry.buffPb.buff_table_id());
        for (const auto& dispelTag : addBuffRow->dispel_tag() | std::views::keys) {
            if (buffRow->tag().contains(dispelTag)) {
                dispelBuffIdList.emplace_back(buffId);
                break;
            }
        }
    }

    for (const auto& buffId : dispelBuffIdList) {
        BuffSystem::OnBuffExpire(parent, buffId);
    }

    return addBuffRow->buff_type() == kBuffTypeDispel;
}

void BuffSystem::OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable)
{
    // Frozen target: buff applied to a player who is mid cross-zone migration.
    // The buff state was captured into PlayerAllData at HandleCrossZoneTransfer
    // and the destination zone will recreate the buff list from the snapshot.
    // Adding a buff on the source side now produces a buff that nobody but
    // this dying source-side entity can see - a leak. Default policy per
    // cross-zone-readiness-audit.md §11.4 is "buff drop".
    if (tlsEcs.actorRegistry.any_of<PlayerFrozenComp>(parent))
    {
        LOG_INFO << "[CrossZone] Buff on frozen target dropped. parent="
                 << entt::to_integral(parent)
                 << " buff_table=" << (buffTable ? buffTable->id() : 0);
        return;
    }

    // Maintain stealth tag cache
    if (buffTable && buffTable->buff_type() == kBuffTypeStealth)
    {
        tlsEcs.actorRegistry.emplace_or_replace<StealthedTagComp>(parent);
    }

    BuffImplSystem::OnBuffStart(parent, buff, buffTable);
    ModifierBuffImplSystem::OnBuffStart(parent, buff, buffTable);
    MotionModifierBuffImplSystem::OnBuffStart(parent, buff, buffTable);
}

void BuffSystem::OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffEntry& buffComp)
{
    // TODO: implement buff refresh logic (e.g. reset duration, reapply modifiers)
}

void BuffSystem::OnBuffRemove(const entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable)
{
    // Maintain stealth tag cache: drop the tag only when no other stealth buff remains
    // (this buff is still in the list at this point, so exclude it from the scan).
    if (buffTable && buffTable->buff_type() == kBuffTypeStealth &&
        !HasStealthBuffExcept(parent, buffComp.buffPb.buff_id()))
    {
        tlsEcs.actorRegistry.remove<StealthedTagComp>(parent);
    }

    ModifierBuffImplSystem::OnBuffRemove(parent, buffComp, buffTable);
    MotionModifierBuffImplSystem::OnBuffRemove(parent, buffComp, buffTable);
}

void BuffSystem::OnBuffDestroy(entt::entity parent, const uint64_t buffId, const BuffTable* buffTable)
{
    BuffImplSystem::OnBuffDestroy(parent, buffId, buffTable);
}

// Buff periodic interval handler
void BuffSystem::OnIntervalThink(entt::entity parent, uint64_t buffId)
{
    auto &buffList = tlsEcs.actorRegistry.get<BuffListComp>(parent);
    const auto buffIt = buffList.find(buffId);

    if (buffIt == buffList.end()) {
        LOG_ERROR << "Cannot find buff " << buffId;
        return;
    }

    const auto buffTableId = buffIt->second.buffPb.buff_table_id();
    LookupBuffOrReturnVoid(buffTableId);

    BuffImplSystem::OnIntervalThink(parent, buffIt->second, buffRow);
    ModifierBuffImplSystem::OnIntervalThink(parent, buffIt->second, buffRow);
    MotionModifierBuffImplSystem::OnIntervalThink(parent, buffIt->second, buffRow);
}
void BuffSystem::OnSkillExecuted(SkillExecutedEvent& event)
{
    // TODO: implement buff reactions to skill execution events
}

void BuffSystem::OnBeforeGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent)
{
    BuffImplSystem::OnBeforeGiveDamage(casterEntity, targetEntity, damageEvent);
}


void BuffSystem::OnAfterGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent)
{
    // TODO: implement post-damage-dealt buff triggers
}

void BuffSystem::OnBeforeTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent)
{
    // TODO: implement pre-damage-taken buff triggers (e.g. damage reduction shields)
}

void BuffSystem::OnAfterTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent)
{
    // casterEntity is the entity that just took damage -> reset its combat-idle buff.
    BuffImplSystem::ResetCombatIdleBuff(entt::to_entity(damageEvent.attacker_id()), casterEntity);
}

void BuffSystem::OnBeforeDead(entt::entity parent)
{
    // TODO: implement pre-death buff triggers (e.g. death prevention, last stand)
}

void BuffSystem::OnAfterDead(entt::entity parent)
{
    // TODO: implement post-death buff cleanup or triggers
}

void BuffSystem::OnKill(entt::entity parent)
{
    // TODO: implement on-kill buff triggers (e.g. life steal, kill streak buffs)
}

void BuffSystem::OnSkillHit(const entt::entity casterEntity, const entt::entity targetEntity) {
    BuffImplSystem::OnSkillHit(casterEntity, targetEntity);
    ModifierBuffImplSystem::OnSkillHit(casterEntity, targetEntity);
    MotionModifierBuffImplSystem::OnSkillHit(casterEntity, targetEntity);
}

bool BuffSystem::AddSubBuffs(entt::entity parent,
    const BuffTable* buffTable,
    BuffEntry& buffComp)
{
    if (buffTable == nullptr)
    {
        return false;
    }

    // Skip if sub-buffs already added
    if (buffComp.buffPb.has_added_sub_buff()) {
        return false;
    }

    buffComp.buffPb.set_has_added_sub_buff(true);
    AddSubBuffsWithoutCheck(parent, buffTable, buffComp);
    return true;
}

void BuffSystem::AddTargetSubBuffs(const entt::entity targetEntity,
    const BuffTable* buffTable,
    const SkillContextPtrComp& abilityContext)
{
    if (buffTable == nullptr)
    {
        return;
    }

    for (auto& targetSubBuffTableId : buffTable->target_sub_buff()) {
        AddOrUpdateBuff(targetEntity, targetSubBuffTableId, abilityContext);
    }
}

void BuffSystem::AddSubBuffsWithoutCheck(entt::entity parent,
    const BuffTable* buffTable,
    BuffEntry& buffComp)
{
    for (const auto& subBuff : buffTable->sub_buff()) {
        auto [result, newBuffId] = BuffSystem::AddOrUpdateBuff(parent, subBuff, buffComp.skillContext);

        if (result != kSuccess || newBuffId == UINT64_MAX) {
            continue;
        }

        // Add to sub-buff list (false = not yet activated)
        buffComp.buffPb.mutable_sub_buff_list_id()->emplace(newBuffId, false);
    }
}

bool CanApplyMoreTicks(const BuffPeriodicBuffComp& periodicBuff, const BuffTable* buffTable) {
    return (buffTable->interval_count() == 0) || (periodicBuff.ticks_done() + 1 <= buffTable->interval_count());
}

void UpdatePeriodicBuff(const entt::entity target, const uint64_t buffId, BuffEntry& buffComp, double delta) {
    LookupBuffOrReturnVoid(buffComp.buffPb.buff_table_id());

    if (buffRow->interval() <= 0) {
        return;
    }

    auto& periodicBuff = *buffComp.buffPb.mutable_periodic();
    double periodicTimer = periodicBuff.periodic_timer() + delta;

    for (uint32_t i = 0; i < 5 && CanApplyMoreTicks(periodicBuff, buffRow); ++i ) {
        if (periodicTimer < buffRow->interval()) {
            break;
        }
        
        periodicTimer -= buffRow->interval();
        periodicBuff.set_ticks_done(periodicBuff.ticks_done() + 1);
        BuffSystem::OnIntervalThink(target, buffId);
    }

    periodicBuff.set_periodic_timer(periodicTimer);
}

void ProcessBuffs(const entt::entity target, BuffListComp& buffListComp, const double delta) {
    for (auto& [buffId, buffComp] : buffListComp) {
        UpdatePeriodicBuff(target, buffId, buffComp, delta);
    }
}

void BuffSystem::Update(const double delta) {
    // PlayerFrozenComp exclude: buff timers must NOT keep counting down on
    // the source side while the player is mid-cross-zone-migration. The
    // marshaled PlayerAllData carries the buff list with its current
    // remaining time; if we kept ticking here a 30s buff could expire on
    // the source mid-flight and the destination would resurrect it as if
    // fresh.  cross-zone-readiness-audit.md §11.2.
    for (auto&& [target, buffListComp] : tlsEcs.actorRegistry.view<BuffListComp>(
            entt::exclude<PlayerFrozenComp>).each()) {
        ProcessBuffs(target, buffListComp, delta);
        BuffSystem::RemovePendingBuffs(target, buffListComp);
    }
}


