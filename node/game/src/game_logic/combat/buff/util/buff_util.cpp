#include "buff_util.h"

#include <ranges>
#include <muduo/base/Logging.h>
#include "buff_config.h"
#include "buff_error_tip.pb.h"
#include "common_error_tip.pb.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "macros/return_define.h"
#include "proto/logic/event/skill_event.pb.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "util/utility.h"

uint64_t GenerateUniqueBuffId(const BuffListComp& buffList)
{
    uint64_t newBuffId;
    do {
        newBuffId = tlsGame.buffIdGenerator.Generate();
    } while (buffList.contains(newBuffId));
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

uint32_t BuffUtil::AddOrUpdateBuff(const entt::entity parent, const uint32_t buffTableId, const SkillContextPtrComp& abilityContext)
{
    auto [buffTable, result] = GetBuffTable(buffTableId);
    if (!buffTable) {
        return result;
    }

    result = CanCreateBuff(parent, buffTableId);
    CHECK_RETURN_IF_NOT_OK(result);

    auto& buffList = tls.registry.get<BuffListComp>(parent);

    if (HandleExistingBuff(parent, buffTableId, abilityContext)) {
        return kOK;
    }

    BuffComp newBuff;
    newBuff.buffPb.set_caster(abilityContext->caster());
    newBuff.buffPb.set_processed_caster(buffTable->nocaster() ? entt::null : abilityContext->caster());

    if ( !OnBuffAwake(parent, buffTableId) ) {
        return kOK;
    }
    
    uint64_t newBuffId = GenerateUniqueBuffId(buffList);
    newBuff.buffPb.set_buff_id(newBuffId);
    newBuff.abilityContext = abilityContext;

    auto [fst, snd] = buffList.emplace(newBuffId, std::move(newBuff));
    OnBuffStart(parent, newBuffId);

    if (buffTable->duration() > 0) {
        fst->second.expireTimerTaskComp.RunAfter(buffTable->duration(), [parent, newBuffId] {
            OnBuffExpire(parent, newBuffId);
        });
    } else if (IsZero(buffTable->duration())) {//todo std::is_zero
        OnBuffExpire(parent, newBuffId);
    }

    return kOK;
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
    
    OnBuffRemove(parent, buffId);
    buffList.erase(buffId);
    OnBuffDestroy(parent, buffTableId);
}

uint32_t BuffUtil::CanCreateBuff(entt::entity parent, uint32_t buffTableId)
{
    auto [buffTable, result] = GetBuffTable(buffTableId);
    if (result != kOK) {
        return result;
    }

    const auto& buffList = tls.registry.get<BuffListComp>(parent);
    const bool isImmune = IsTargetImmune(buffList, buffTable);

    return isImmune ? kBuffTargetImmuneToBuff : kOK;
}

bool BuffUtil::HandleExistingBuff(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext)
{
    auto& buffList = tls.registry.get<BuffListComp>(parent);
    for (auto& buffComp : buffList | std::views::values) {
        if (buffComp.buffPb.buff_table_id() == buffTableId && buffComp.abilityContext->caster() == abilityContext->caster()) {
            if (buffComp.buffPb.layer() < GetBuffTable(buffTableId).first->maxlayer()) {
                buffComp.buffPb.set_layer(buffComp.buffPb.layer() + 1);
            }
            OnBuffRefresh(parent, buffTableId, abilityContext, buffComp);
            return true;
        }
    }
    return false;
}

bool BuffUtil::OnBuffAwake(entt::entity parent, uint32_t buffTableId){
    auto [newBuffTable, result] = GetBuffTable(buffTableId);
    if (!newBuffTable) {
        return result;
    }

    UInt64Vector removeBuffIdList;

    for (auto& buffList = tls.registry.get<BuffListComp>(parent); auto& [buffId, buffPbComp] : buffList){
        auto [buffTable, result] = GetBuffTable(buffTableId);
        if (!buffTable) {
            continue;
        }

        for (auto& [removeTag, _] : newBuffTable->removetag()){
            if (buffTable->tag().contains(removeTag)){
                removeBuffIdList.emplace_back(buffId);
                break;
            }
        }
    }

    for (const auto& buffId : removeBuffIdList){
        BuffUtil::OnBuffExpire(parent, buffId);
    }
    return false;
}

void BuffUtil::OnBuffStart(entt::entity parent, uint64_t buffId)
{
}

void BuffUtil::OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffComp& buffComp)
{
    // Implement logic if needed
}

void BuffUtil::OnBuffRemove(entt::entity parent, uint64_t buffId)
{
    // Implement logic if needed
}

void BuffUtil::OnBuffDestroy(entt::entity parent, uint32_t buffTableId)
{
    // Implement logic if needed
}

void BuffUtil::OnIntervalThink(entt::entity parent, uint64_t buffId)
{
    // Implement interval logic if needed
}

void BuffUtil::OnSkillExecuted(SkillExecutedEvent& event)
{
    // Implement event handling logic
}

void BuffUtil::ApplyMotion()
{

}

void BuffUtil::OnMotionUpdate()
{

}

void BuffUtil::OnMotionInterrupt()
{

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