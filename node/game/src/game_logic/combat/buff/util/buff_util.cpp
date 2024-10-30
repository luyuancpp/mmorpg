#include "buff_util.h"

#include <ranges>
#include <muduo/base/Logging.h>
#include "buff_config.h"
#include "buff_error_tip.pb.h"
#include "buff_impl_util.h"
#include "common_error_tip.pb.h"
#include "modifier_buff_util.h"
#include "motion_modifier_util.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "proto/logic/event/skill_event.pb.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "util/utility.h"

uint64_t GenerateUniqueBuffId(const BuffListComp& buffList)
{
    uint64_t newBuffId = UINT64_MAX;
    do {
        newBuffId = tlsGame.buffIdGenerator.Generate();
    } while (buffList.contains(newBuffId) || newBuffId == UINT64_MAX);
    return newBuffId;
}

bool IsTargetImmune(const BuffListComp& buffList, const BuffTable* buffTable)
{
    for (const auto& buff : buffList | std::views::values) {
        auto [currentBuffTable, fetchResult] = GetBuffTable(buff.buffPb.buff_table_id());
        if (fetchResult != kOK) {
            return true;
        }

        for (const auto& tag : buffTable->tag() | std::views::keys) {
            if (currentBuffTable->immunetag().contains(tag)) {
                return true;
            }
        }
    }
    return false;
}

void BuffUtil::InitializeActorComponents(entt::entity entity)
{
    tls.registry.emplace<BuffListComp>(entity);
}

BuffMessagePtr CreateBuffDataPtr(const BuffTable* buffTable) {
    switch (buffTable->bufftype()) {
    case kBuffTypeNoDamageOrSkillHitInLastSeconds:
        {
            return std::make_shared<BuffNoDamageOrSkillHitInLastSecondsPbComp>();
        }
        break;
    default:
        return nullptr;
        break;
    }
    
    return  nullptr;
}

std::tuple<uint32_t, uint64_t>  BuffUtil::AddOrUpdateBuff(const entt::entity parent, const uint32_t buffTableId, const SkillContextPtrComp& abilityContext)
{
    auto [buffTable, result] = GetBuffTable(buffTableId);
    if (!buffTable) {
        return std::make_tuple<uint32_t, uint64_t>(std::move(result), UINT64_MAX);
    }

    result = CanCreateBuff(parent, buffTableId);
    if(result != kOK){
        return std::make_tuple<uint32_t, uint64_t>(std::move(result), UINT64_MAX);
    }

    auto& buffList = tls.registry.get<BuffListComp>(parent);

    if (HandleExistingBuff(parent, buffTableId, abilityContext)) {
        return std::make_tuple<uint32_t, uint64_t>(std::move(result), UINT64_MAX);
    }

    BuffComp newBuff;
    newBuff.buffPb.set_caster(abilityContext->caster());
    newBuff.buffPb.set_processed_caster(buffTable->nocaster() ? entt::null : abilityContext->caster());

    if (!OnBuffAwake(parent, buffTableId)) {
        return std::make_tuple<uint32_t, uint64_t>(std::move(result), UINT64_MAX);
    }

    uint64_t newBuffId = GenerateUniqueBuffId(buffList);
    newBuff.buffPb.set_buff_id(newBuffId);
    newBuff.skillContext = abilityContext;

    newBuff.dataPbPtr = CreateBuffDataPtr(buffTable);

    auto [fst, snd] = buffList.emplace(newBuffId, std::move(newBuff));
    OnBuffStart(parent, fst->second, buffTable);

    if (buffTable->duration() > 0) {
        fst->second.expireTimerTaskComp.RunAfter(buffTable->duration(), [parent, newBuffId] {
            OnBuffExpire(parent, newBuffId);
        });
    } else if (IsZero(buffTable->duration())) {
        OnBuffExpire(parent, newBuffId);
    }

    return std::make_tuple<uint32_t, uint64_t>(kOK, std::move(newBuffId));
}

void BuffUtil::RemoveBuff(const entt::entity parent, const uint64_t buffId){
    OnBuffExpire(parent, buffId);
}

void BuffUtil::OnBuffExpire(const entt::entity parent, const uint64_t buffId)
{
    auto& buffList = tls.registry.get<BuffListComp>(parent);
    const auto buffIt = buffList.find(buffId);

    if (buffIt == buffList.end()) {
        LOG_ERROR << "Cannot find buff " << buffId;
        return;
    }

    const auto buffTableId = buffIt->second.buffPb.buff_table_id();

    auto [buffTable, result] = GetBuffTable(buffTableId);
    if (!buffTable) {
        return ;
    }
    
    OnBuffRemove(parent, buffIt->second, buffTable);
    buffList.erase(buffId);
    OnBuffDestroy(parent, buffIt->second, buffTable);
}

uint32_t BuffUtil::CanCreateBuff(entt::entity parent, uint32_t buffTableId)
{
    auto [buffTable, result] = GetBuffTable(buffTableId);
    if (result != kOK) {
        return result;
    }

    const auto& buffList = tls.registry.get<BuffListComp>(parent);

    if (const bool isImmune = IsTargetImmune(buffList, buffTable)){
        return kBuffTargetImmuneToBuff;
    }
    
    return kOK;
}

bool BuffUtil::HandleExistingBuff(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext)
{
    for (auto& buffList = tls.registry.get<BuffListComp>(parent); auto& buffComp : buffList | std::views::values) {
        if (buffComp.buffPb.buff_table_id() == buffTableId && buffComp.buffPb.processed_caster() == abilityContext->caster()) {
            if (buffComp.buffPb.layer() < GetBuffTable(buffTableId).first->maxlayer()) {
                buffComp.buffPb.set_layer(buffComp.buffPb.layer() + 1);
            }
            OnBuffRefresh(parent, buffTableId, abilityContext, buffComp);
            return true;
        }
    }
    return false;
}

bool BuffUtil::OnBuffAwake(const entt::entity parent, const uint32_t buffTableId){
    auto [newBuffTable, result] = GetBuffTable(buffTableId);
    if (!newBuffTable) {
        return result;
    }

    UInt64Vector dispelBuffIdList;

    for (auto& buffList = tls.registry.get<BuffListComp>(parent); auto& [buffId, buffPbComp] : buffList){
        auto [buffTable, result] = GetBuffTable(buffTableId);
        if (!buffTable) {
            continue;
        }

        for (const auto& removeTag : newBuffTable->dispeltag() | std::views::keys){
            if (buffTable->tag().contains(removeTag)){
                dispelBuffIdList.emplace_back(buffId);
                break;
            }
        }
    }

    for (const auto& buffId : dispelBuffIdList){
        BuffUtil::OnBuffExpire(parent, buffId);
    }
    
    return newBuffTable->bufftype() == kBuffTypeDispel;
}

void BuffUtil::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable){
    if (ModifierBuffUtil::OnBuffStart(parent, buff, buffTable)) {
        return;
    } else if (MotionModifierBuffUtil::OnBuffStart(parent, buff, buffTable))
    {
        return;
    }
}

void BuffUtil::OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffComp& buffComp)
{
    // Implement logic if needed
}

void BuffUtil::OnBuffRemove(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
{
    if (ModifierBuffUtil::OnBuffRemove(parent, buffComp, buffTable)){
        return;
    }else if ( ModifierBuffUtil::OnBuffRemove(parent, buffComp, buffTable)){
        return;
    }
}

void BuffUtil::OnBuffDestroy(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
{
    // Implement logic if needed
}

void BuffUtil::OnIntervalThink(entt::entity parent, uint64_t buffId)
{
    auto& buffList = tls.registry.get<BuffListComp>(parent);
    const auto buffIt = buffList.find(buffId);

    if (buffIt == buffList.end()) {
        LOG_ERROR << "Cannot find buff " << buffId;
        return;
    }

    const auto buffTableId = buffIt->second.buffPb.buff_table_id();

    auto [buffTable, result] = GetBuffTable(buffTableId);
    if (!buffTable) {
        return ;
    }
    
    if (ModifierBuffUtil::OnIntervalThink(parent, buffIt->second, buffTable)){
        return;
    }else if ( ModifierBuffUtil::OnIntervalThink(parent, buffIt->second, buffTable)){
        return;
    }
    // Implement interval logic if needed
}

void BuffUtil::OnSkillExecuted(SkillExecutedEvent& event)
{
    // Implement event handling logic
}

void BuffUtil::OnBeforeGiveDamage(entt::entity parent, DamageEventPbComponent& damageEvent)
{
    //class Buff {
    //public:
    //	bool HasFlag(int flag) const;
    //	bool HasChanceForDOT() const;
    //	// 添加DOT Buff的实现
    //	void AddDOTBuff(entt::entity target);
    //};
    // 
    // 检查并应用Buff效果
    //	for (auto& buff : tls.registry.get<BuffListComp>(event.target)) {
    //		if (buff.second.HasFlag(DamageFlag_NotMiss)) {
    //			event.damageFlags |= DamageFlag_NotMiss;
    //		}
    //	}
}


void BuffUtil::OnAfterGiveDamage(entt::entity parent, DamageEventPbComponent& damageEvent)
{
    // 检查并应用DOT效果
    //for (auto& buff : tls.registry.get<BuffListComp>(event.target)) {
    //	if (buff.second.HasChanceForDOT()) {
    //		// 添加DOT Buff
    //		AddDOTBuff(event.target);
    //	}
    //}
}

void BuffUtil::OnBeforeTakeDamage(entt::entity parent, DamageEventPbComponent& damageEvent)
{
    //auto& buffs = tls.registry.get<BuffListComp>(event.target);
    //for (auto& [buffId, buff] : buffs) {
    //	if (buff.HasShield()) {
    //		// 假设护盾Buff会减少伤害
    //		event.damageAmount *= 0.5f; // 示例：护盾减少50%伤害
    //	}
    //}
}

void BuffUtil::OnAfterTakeDamage(entt::entity parent, DamageEventPbComponent& damageEvent)
{
    BuffImplUtil::UpdateLastDamageOrSkillHitTime(entt::to_entity(damageEvent.attacker_id()), parent);

    // 检查并应用额外效果
    //auto& buffs = tls.registry.get<BuffListComp>(event.target);
    //for (auto& [buffId, buff] : buffs) {
    //	if (buff.HasPostDamageEffect()) {
    //		// 执行额外效果，例如添加额外的伤害效果
    //		ApplyPostDamageEffects(event.target);
    //	}
    //}
}

void BuffUtil::OnBeforeDead(entt::entity parent)
{

}

void BuffUtil::OnAfterDead(entt::entity parent)
{

}

void BuffUtil::OnKill(entt::entity parent)
{

}

void BuffUtil::OnSkillHit(const entt::entity casterEntity, const entt::entity targetEntity){
    BuffImplUtil::UpdateLastDamageOrSkillHitTime(casterEntity, targetEntity);
    
    if (ModifierBuffUtil::OnSkillHit(casterEntity, targetEntity)){
        return;
    } else if (MotionModifierBuffUtil::OnSkillHit(casterEntity, targetEntity))
    {
        return;
    }
}
