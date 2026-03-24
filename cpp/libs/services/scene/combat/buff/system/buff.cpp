#include "buff.h"
#include <ranges>
#include "table/code/buff_table.h"
#include "combat/buff/system/buff_impl.h"
#include "proto/common/component/buff_comp.pb.h"
#include <muduo/base/Logging.h>
#include "table/proto/tip/buff_error_tip.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "modifier_buff_impl.h"
#include "motion_modifier_impl.h"
#include "combat/buff/comp/buff.h"
#include "combat/buff/constants/buff.h"
#include "proto/common/event/skill_event.pb.h"
#include "core/utils/utility/utility.h"
#include "core/system/id_generator.h"
#include <thread_context/registry_manager.h>


// TODO: Combat logic must run on frames, not timers. This ensures buff triggers and expirations
// happen within frame logic, avoiding issues where timer callbacks fire after entity
// destruction or buff removal. Consider migrating all buff expiry and periodic triggers
// to per-frame processing.

// Generate unique buff ID
uint64_t GenerateUniqueBuffId(const BuffListComp& buffList)
{
    uint64_t newBuffId = UINT64_MAX;
    do {
        newBuffId = tlsIdGeneratorManager.buffIdGenerator.Generate();
    } while (buffList.contains(newBuffId) || newBuffId == UINT64_MAX);
    return newBuffId;
}

// Check target immunity
bool IsTargetImmune(const BuffListComp& buffList, const BuffTable* buffTableParam)
{
    for (const auto& buff : buffList | std::views::values) {
        FetchAndValidateBuffTable(buff.buffPb.buff_table_id());
        for (const auto& tag : buffTableParam->tag() | std::views::keys) {
            if (buffTable->immunetag().contains(tag)) {
                return true;
            }
        }
    }
    return false;
}

// Create buff data pointer
BuffMessagePtr CreateBuffDataPtr(const BuffTable* buffTable) {
    switch (buffTable->bufftype()) {
    case kBuffTypeNoDamageOrSkillHitInLastSeconds:
        return std::make_shared<BuffNoDamageOrSkillHitInLastSecondsComp>();
    default:
        return nullptr;
    }
}

// Add or update buff (with abilityContext)
std::tuple<uint32_t, uint64_t> BuffSystem::AddOrUpdateBuff(
    const entt::entity parent,
    const uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext)
{
    if (!tlsRegistryManager.actorRegistry.valid(parent))
    {
        return {kThisEntityIsInvalid, UINT64_MAX};
    }

    FetchBuffTableOrReturnCustom(buffTableId, (std::make_tuple(fetchResult, UINT64_MAX)));

    auto result = CanCreateBuff(parent, buffTableId);
    if (result != kSuccess) {
        return {result, UINT64_MAX};
    }

    auto& buffList = tlsRegistryManager.actorRegistry.get_or_emplace<BuffListComp>(parent);

    BuffEntry newBuff;
    if (abilityContext != nullptr)
    {
        newBuff.buffPb.set_caster(abilityContext->caster());
    }
    newBuff.buffPb.set_processed_caster(buffTable->nocaster() ? entt::null : (abilityContext ? abilityContext->caster() : entt::null));

    if (OnBuffAwake(parent, buffTableId) == kSuccess) {
        return {result, UINT64_MAX};
    }

    if (HandleExistingBuff(parent, buffTableId, abilityContext)) {
        return {result, UINT64_MAX};
    }

    uint64_t newBuffId = GenerateUniqueBuffId(buffList);
    newBuff.buffPb.set_buff_id(newBuffId);
    newBuff.buffPb.set_buff_table_id(buffTableId);
    newBuff.skillContext = abilityContext;
    newBuff.dataPbPtr = CreateBuffDataPtr(buffTable);

    auto [fst, snd] = buffList.emplace(newBuffId, std::move(newBuff));
    OnBuffStart(parent, fst->second, buffTable);

    if (buffTable->duration() > 0) {
        fst->second.expireTimerTaskComp.RunAfter(buffTable->duration(), [parent, newBuffId] {
            if (!tlsRegistryManager.actorRegistry.valid(parent))
            {
                return;
            }
            OnBuffExpire(parent, newBuffId);
            });
    }
    else if (IsZero(buffTable->duration())) {
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
    auto& pendingRemoveBuffs = tlsRegistryManager.actorRegistry.get_or_emplace<BuffPendingRemoveBuffs>(parent);
    pendingRemoveBuffs.emplace(buffId);
}

// Remove pending buffs at end of frame
void BuffSystem::RemovePendingBuffs(const entt::entity parent, BuffListComp& buffListComp) {
    auto& pendingRemoveBuffs = tlsRegistryManager.actorRegistry.get_or_emplace<BuffPendingRemoveBuffs>(parent);

    for (const auto& buffId : pendingRemoveBuffs) {
        buffListComp.erase(buffId);
        LOG_TRACE << "Buff with ID " << buffId << " removed from entity at end of frame.\n";
    }

    pendingRemoveBuffs.clear();
}

// Buff expiry handler
void BuffSystem::OnBuffExpire(const entt::entity parent, const uint64_t buffId)
{
    auto& buffList = tlsRegistryManager.actorRegistry.get_or_emplace<BuffListComp>(parent);
    const auto buffIt = buffList.find(buffId);

    if (buffIt == buffList.end()) {
        LOG_ERROR << "Cannot find buff " << buffId;
        return;
    }

    const auto buffTableId = buffIt->second.buffPb.buff_table_id();
    FetchBuffTableOrReturnVoid(buffTableId);

    OnBuffRemove(parent, buffIt->second, buffTable);
    buffList.erase(buffId);
    OnBuffDestroy(parent, buffId, buffTable);
}

// Check if buff can be created
uint32_t BuffSystem::CanCreateBuff(const entt::entity parentEntity, const uint32_t buffTableId)
{
    FetchAndValidateBuffTable(buffTableId);

    const auto& buffList = tlsRegistryManager.actorRegistry.get_or_emplace<BuffListComp>(parentEntity);
    if (const bool isImmune = IsTargetImmune(buffList, buffTable)) {
        return kBuffTargetImmuneToBuff;
    }

    return kSuccess;
}

// Handle existing buff (stack/refresh)
bool BuffSystem::HandleExistingBuff(const entt::entity parentEntity,
    const uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext)
{
    FetchBuffTableOrReturnFalse(buffTableId);

    auto& buffList = tlsRegistryManager.actorRegistry.get_or_emplace<BuffListComp>(parentEntity);
    for (auto& buffComp : buffList | std::views::values) {
        if (buffComp.buffPb.buff_table_id() == buffTableId && buffComp.buffPb.processed_caster() == abilityContext->caster()) {
            if (buffComp.buffPb.layer() < buffTable->maxlayer()) {
                buffComp.buffPb.set_layer(buffComp.buffPb.layer() + 1);
            }
            OnBuffRefresh(parentEntity, buffTableId, abilityContext, buffComp);
            return true;
        }
    }
    return false;
}

// Buff awake handler (dispel on instantiation, before activation)
uint32_t BuffSystem::OnBuffAwake(const entt::entity parent, const uint32_t buffTableId)
{
    FetchAndValidateCustomBuffTable(add, buffTableId);

    UInt64Vector dispelBuffIdList;
    auto& buffList = tlsRegistryManager.actorRegistry.get_or_emplace<BuffListComp>(parent);
    for (auto& [buffId, buffPbComp] : buffList) {
        FetchBuffTableOrContinue(buffTableId);
        for (const auto& removeTag : addBuffTable->dispeltag() | std::views::keys) {
            if (buffTable->tag().contains(removeTag)) {
                dispelBuffIdList.emplace_back(buffId);
                break;
            }
        }
    }

    for (const auto& buffId : dispelBuffIdList) {
        BuffSystem::OnBuffExpire(parent, buffId);
    }

    if (addBuffTable->bufftype() != kBuffTypeDispel) {
        return kInvalidTableData;
    }

    return kSuccess;
}

void BuffSystem::OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable)
{
    if (BuffImplSystem::OnBuffStart(parent, buff, buffTable)) {
        return;
    }else if (ModifierBuffImplSystem::OnBuffStart(parent, buff, buffTable)) {
        return;
    }
    else if (MotionModifierBuffImplSystem::OnBuffStart(parent, buff, buffTable)) {
        return;
    }
}

void BuffSystem::OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffEntry& buffComp) {}

void BuffSystem::OnBuffRemove(const entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable)
{
    if (ModifierBuffImplSystem::OnBuffRemove(parent, buffComp, buffTable)) {
        return;
    }
    else if (MotionModifierBuffImplSystem::OnBuffRemove(parent, buffComp, buffTable)) {
        return;
    }
}

void BuffSystem::OnBuffDestroy(entt::entity parent, const uint64_t buffId, const BuffTable* buffTable)
{
    if (BuffImplSystem::OnBuffDestroy(parent, buffId, buffTable)) {
        return;
    }
}

// Buff periodic interval handler
void BuffSystem::OnIntervalThink(entt::entity parent, uint64_t buffId)
{
    auto& buffList = tlsRegistryManager.actorRegistry.get_or_emplace<BuffListComp>(parent);
    const auto buffIt = buffList.find(buffId);

    if (buffIt == buffList.end()) {
        LOG_ERROR << "Cannot find buff " << buffId;
        return;
    }

    const auto buffTableId = buffIt->second.buffPb.buff_table_id();
    FetchBuffTableOrReturnVoid(buffTableId);

    if (BuffImplSystem::OnIntervalThink(parent, buffIt->second, buffTable)) {
        return;
    }
    else if (ModifierBuffImplSystem::OnIntervalThink(parent, buffIt->second, buffTable)) {
        return;
    }
    else if (MotionModifierBuffImplSystem::OnIntervalThink(parent, buffIt->second, buffTable)) {
        return;
    }
}
void BuffSystem::OnSkillExecuted(SkillExecutedEvent& event)
{
}

void BuffSystem::OnBeforeGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent)
{
    BuffImplSystem::OnBeforeGiveDamage(casterEntity, targetEntity, damageEvent);
}


void BuffSystem::OnAfterGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent)
{
}

void BuffSystem::OnBeforeTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent)
{
}

void BuffSystem::OnAfterTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent)
{
    BuffImplSystem::UpdateLastDamageOrSkillHitTime(entt::to_entity(damageEvent.attacker_id()), casterEntity);
}

void BuffSystem::OnBeforeDead(entt::entity parent)
{

}

void BuffSystem::OnAfterDead(entt::entity parent)
{

}

void BuffSystem::OnKill(entt::entity parent)
{

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

    for (const auto& subBuff : buffTable->subbuff()) {
        auto [result, newBuffId] = BuffSystem::AddOrUpdateBuff(parent, subBuff, buffComp.skillContext);

        if (result != kSuccess || newBuffId == UINT64_MAX) {
            continue;
        }

        // Add to sub-buff list (false = not yet activated)
        buffComp.buffPb.mutable_sub_buff_list_id()->emplace(newBuffId, false);
    }

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

    for (auto& targetSubBuffTableId : buffTable->targetsubbuff()) {
        AddOrUpdateBuff(targetEntity, targetSubBuffTableId, abilityContext);
    }
}

void BuffSystem::AddSubBuffsWithoutCheck(entt::entity parent,
    const BuffTable* buffTable,
    BuffEntry& buffComp)
{
    for (const auto& subBuff : buffTable->subbuff()) {
        auto [result, newBuffId] = BuffSystem::AddOrUpdateBuff(parent, subBuff, buffComp.skillContext);

        if (result != kSuccess || newBuffId == UINT64_MAX) {
            continue;
        }

        // Add to sub-buff list (false = not yet activated)
        buffComp.buffPb.mutable_sub_buff_list_id()->emplace(newBuffId, false);
    }
}

bool CanApplyMoreTicks(const BuffPeriodicBuffComp& periodicBuff, const BuffTable* buffTable) {
    return (buffTable->intervalcount() <= 0) || (periodicBuff.ticks_done() + 1 <= buffTable->intervalcount());
}

void UpdatePeriodicBuff(const entt::entity target, const uint64_t buffId, BuffEntry& buffComp, double delta) {
    FetchBuffTableOrReturnVoid(buffComp.buffPb.buff_table_id());

    if (buffTable->interval() <= 0) {
        return;
    }

    auto& periodicBuff = *buffComp.buffPb.mutable_periodic();
    double periodicTimer = periodicBuff.periodic_timer() + delta;

    for (uint32_t i = 0; i < 5 && CanApplyMoreTicks(periodicBuff, buffTable); ++i ) {
        if (periodicTimer < buffTable->interval()) {
            break;
        }
        
        periodicTimer -= buffTable->interval();
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
    for (auto&& [target, buffListComp] : tlsRegistryManager.actorRegistry.view<BuffListComp>().each()) {
        ProcessBuffs(target, buffListComp, delta);
        BuffSystem::RemovePendingBuffs(target, buffListComp);
    }
}


