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

// Buff ID生成逻辑
uint64_t GenerateUniqueBuffId(const BuffListComp& buffList)
{
    uint64_t newBuffId = UINT64_MAX;
    do {
        newBuffId = tlsGame.buffIdGenerator.Generate();
    } while (buffList.contains(newBuffId) || newBuffId == UINT64_MAX);
    return newBuffId;
}

// 目标免疫判定
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

// 初始化组件
void BuffUtil::InitializeActorComponents(entt::entity entity)
{
    tls.registry.emplace<BuffListComp>(entity);
    tls.registry.emplace<BuffPendingRemoveBuffs>(entity);
}

// 创建 Buff 数据指针
BuffMessagePtr CreateBuffDataPtr(const BuffTable* buffTable) {
    switch (buffTable->bufftype()) {
    case kBuffTypeNoDamageOrSkillHitInLastSeconds:
        return std::make_shared<BuffNoDamageOrSkillHitInLastSecondsPbComp>();
    default:
        return nullptr;
    }
}

// 添加或更新 Buff（有 abilityContext 参数的版本）
std::tuple<uint32_t, uint64_t> BuffUtil::AddOrUpdateBuff(
    const entt::entity parent, 
    const uint32_t buffTableId, 
    const SkillContextPtrComp& abilityContext)
{
    FetchBuffTableOrReturnCustom(buffTableId, (std::make_tuple(fetchResult, UINT64_MAX)));

    auto result = CanCreateBuff(parent, buffTableId);
    if(result != kSuccess){
        return std::make_tuple<uint32_t, uint64_t>(std::move(result), UINT64_MAX);
    }

    auto& buffList = tls.registry.get<BuffListComp>(parent);

    if (HandleExistingBuff(parent, buffTableId, abilityContext)) {
        return std::make_tuple<uint32_t, uint64_t>(std::move(result), UINT64_MAX);
    }

    BuffComp newBuff;
    if (nullptr != abilityContext)
    {
        newBuff.buffPb.set_caster(abilityContext->caster());
    }
    newBuff.buffPb.set_processed_caster(buffTable->nocaster() ? entt::null : (abilityContext ? abilityContext->caster() : entt::null));

    if (!OnBuffAwake(parent, buffTableId)) {
        return std::make_tuple<uint32_t, uint64_t>(std::move(result), UINT64_MAX);
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

            if (!tls.registry.valid(parent))
            {
                return;
            }

            OnBuffExpire(parent, newBuffId);
        });
    } else if (IsZero(buffTable->duration())) {
        OnBuffExpire(parent, newBuffId);
    }

    return std::make_tuple<uint32_t, uint64_t>(kSuccess, std::move(newBuffId));
}

// 添加或更新 Buff（无 abilityContext 参数的版本）
std::tuple<uint32_t, uint64_t> BuffUtil::AddOrUpdateBuff(
    const entt::entity parent, 
    const uint32_t buffTableId)
{
    return AddOrUpdateBuff(parent, buffTableId, nullptr);
}


// 移除 Buff
void BuffUtil::RemoveBuff(const entt::entity parent, const uint64_t buffId)
{
    OnBuffExpire(parent, buffId);
}

void BuffUtil::RemoveBuff(const entt::entity parent, const UInt64Set& removeBuffIdList){
    for (auto& removeBuffId : removeBuffIdList) {
        BuffUtil::RemoveBuff(parent, removeBuffId);
    }
}

void BuffUtil::RemoveSubBuff(BuffComp& buffComp, UInt64Set& buffsToRemove)
{
    for (auto& [subBuffId,_] : buffComp.buffPb.sub_buff_list_id())
    {
        buffsToRemove.emplace(subBuffId);
    }

    buffComp.buffPb.clear_sub_buff_list_id();
}

void BuffUtil::MarkBuffForRemoval(const entt::entity parent, uint64_t buffId) {
    auto& pendingRemoveBuffs = tls.registry.get<BuffPendingRemoveBuffs>(parent);
    pendingRemoveBuffs.emplace(buffId);
}

// 帧结束时统一移除所有待移除的 Buff
void BuffUtil::RemovePendingBuffs(const entt::entity parent, BuffListComp& buffListComp) {
    auto& pendingRemoveBuffs = tls.registry.get<BuffPendingRemoveBuffs>(parent);
    
    for (const auto& buffId : pendingRemoveBuffs) {
        buffListComp.erase(buffId);  // 删除 Buff
        LOG_TRACE << "Buff with ID " << buffId << " removed from entity at end of frame.\n";
    }
    
    pendingRemoveBuffs.clear();
}

// Buff 过期处理
void BuffUtil::OnBuffExpire(const entt::entity parent, const uint64_t buffId)
{
    auto& buffList = tls.registry.get<BuffListComp>(parent);
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

// 检查是否可创建 Buff
uint32_t BuffUtil::CanCreateBuff(const entt::entity parentEntity, const uint32_t buffTableId)
{
    FetchAndValidateBuffTable(buffTableId);  

    const auto& buffList = tls.registry.get<BuffListComp>(parentEntity);
    if (const bool isImmune = IsTargetImmune(buffList, buffTable)){
        return kBuffTargetImmuneToBuff;
    }
    
    return kSuccess;
}

// 处理已存在的 Buff
bool BuffUtil::HandleExistingBuff(const entt::entity parentEntity,
    const uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext)
{
    FetchBuffTableOrReturnFalse(buffTableId);

    for (auto& buffList = tls.registry.get<BuffListComp>(parentEntity); auto& buffComp : buffList | std::views::values) {
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

// Buff 觉醒处理
uint32_t BuffUtil::OnBuffAwake(const entt::entity parent, const uint32_t buffTableId)
{
    FetchAndValidateCustomBuffTable(add, buffTableId);

    UInt64Vector dispelBuffIdList;
    for (auto& buffList = tls.registry.get<BuffListComp>(parent); auto& [buffId, buffPbComp] : buffList){
        FetchBuffTableOrContinue(buffTableId);

        for (const auto& removeTag : addBuffTable->dispeltag() | std::views::keys){
            if (buffTable->tag().contains(removeTag)){
                dispelBuffIdList.emplace_back(buffId);
                break;
            }
        }
    }

    for (const auto& buffId : dispelBuffIdList){
        BuffUtil::OnBuffExpire(parent, buffId);
    }
    
    if (addBuffTable->bufftype() != kBuffTypeDispel) {
        return kInvalidTableData;
    }

    return kSuccess;
}

void BuffUtil::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable)
{
    if (BuffImplUtil::OnBuffStart(parent, buff, buffTable)){
        return;
    }
    
    if (ModifierBuffUtil::OnBuffStart(parent, buff, buffTable)) {
        return;
    } else if (MotionModifierBuffUtil::OnBuffStart(parent, buff, buffTable)) {
        return;
    }
}

void BuffUtil::OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffComp& buffComp) {}

void BuffUtil::OnBuffRemove(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
{
    if (ModifierBuffUtil::OnBuffRemove(parent, buffComp, buffTable)){
        return;
    }else if ( ModifierBuffUtil::OnBuffRemove(parent, buffComp, buffTable)){
        return;
    }
}

void BuffUtil::OnBuffDestroy(entt::entity parent, const uint64_t buffId, const BuffTable* buffTable)
{
    if (BuffImplUtil::OnBuffDestroy(parent, buffId, buffTable)){
        return;
    }
}

// Buff 间隔处理
void BuffUtil::OnIntervalThink(entt::entity parent, uint64_t buffId)
{
    auto& buffList = tls.registry.get<BuffListComp>(parent);
    const auto buffIt = buffList.find(buffId);

    if (buffIt == buffList.end()) {
        LOG_ERROR << "Cannot find buff " << buffId;
        return;
    }

    const auto buffTableId = buffIt->second.buffPb.buff_table_id();
    FetchBuffTableOrReturnVoid(buffTableId);

    if (BuffImplUtil::OnIntervalThink(parent, buffIt->second, buffTable)){
        return;
    }else if (ModifierBuffUtil::OnIntervalThink(parent, buffIt->second, buffTable)){
        return;
    }else if (MotionModifierBuffUtil::OnIntervalThink(parent, buffIt->second, buffTable)){
        return;
    }
}
void BuffUtil::OnSkillExecuted(SkillExecutedEvent& event)
{
    // Implement event handling logic
}

void BuffUtil::OnBeforeGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent)
{
    BuffImplUtil::OnBeforeGiveDamage(casterEntity, targetEntity, damageEvent);
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


void BuffUtil::OnAfterGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent)
{
    // 检查并应用DOT效果
    //for (auto& buff : tls.registry.get<BuffListComp>(event.target)) {
    //	if (buff.second.HasChanceForDOT()) {
    //		// 添加DOT Buff
    //		AddDOTBuff(event.target);
    //	}
    //}
}

void BuffUtil::OnBeforeTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent)
{
    //auto& buffs = tls.registry.get<BuffListComp>(event.target);
    //for (auto& [buffId, buff] : buffs) {
    //	if (buff.HasShield()) {
    //		// 假设护盾Buff会减少伤害
    //		event.damageAmount *= 0.5f; // 示例：护盾减少50%伤害
    //	}
    //}
}

void BuffUtil::OnAfterTakeDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent)
{
    BuffImplUtil::UpdateLastDamageOrSkillHitTime(entt::to_entity(damageEvent.attacker_id()), casterEntity);

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
    BuffImplUtil::OnSkillHit(casterEntity, targetEntity);
    ModifierBuffUtil::OnSkillHit(casterEntity, targetEntity);
    MotionModifierBuffUtil::OnSkillHit(casterEntity, targetEntity);
}

bool BuffUtil::AddSubBuffs(entt::entity parent,
                            const BuffTable* buffTable,
                            BuffComp& buffComp)
{
    if (nullptr == buffTable)
    {
        return false;;
    }
    
    // 如果已经添加过子 Buff，就跳过
    if (buffComp.buffPb.has_added_sub_buff()) {
        return false;
    }

    // 标记为已经添加了子 Buff
    buffComp.buffPb.set_has_added_sub_buff(true);

    // 遍历并添加子 Buff
    for (const auto& subBuff : buffTable->subbuff()) {
        // 调用 BuffUtil 的 AddOrUpdateBuff 进行 Buff 添加或更新
        auto [result, newBuffId] = BuffUtil::AddOrUpdateBuff(parent, subBuff, buffComp.skillContext);

        // 如果 Buff 添加失败或者 Buff ID 无效，则跳过
        if (result != kSuccess || newBuffId == UINT64_MAX) {
            continue;
        }

        // 将新添加的 Buff ID 添加到子 Buff 列表中，初始化为 false 表示未激活
        buffComp.buffPb.mutable_sub_buff_list_id()->emplace(newBuffId, false);
    }

    return true;
}

void BuffUtil::AddTargetSubBuffs(const entt::entity targetEntity,
    const BuffTable* buffTable,
    const SkillContextPtrComp& abilityContext)
{
    if (nullptr == buffTable)
    {
        return;
    }
    
    for (auto& targetSubBuffTableId : buffTable->targetsubbuff()){
        AddOrUpdateBuff(targetEntity, targetSubBuffTableId, abilityContext);
    }
}

// 添加子 Buff，不进行已添加检查
void BuffUtil::AddSubBuffsWithoutCheck(entt::entity parent,
                                       const BuffTable* buffTable,
                                       BuffComp& buffComp)
{
    // 直接添加子 Buff，不检查是否已添加过
    for (const auto& subBuff : buffTable->subbuff()) {
        // 调用 BuffUtil 的 AddOrUpdateBuff 进行 Buff 添加或更新
        auto [result, newBuffId] = BuffUtil::AddOrUpdateBuff(parent, subBuff, buffComp.skillContext);

        // 如果 Buff 添加失败或者 Buff ID 无效，则跳过
        if (result != kSuccess || newBuffId == UINT64_MAX) {
            continue;
        }

        // 将新添加的 Buff ID 添加到子 Buff 列表中，初始化为 false 表示未激活
        buffComp.buffPb.mutable_sub_buff_list_id()->emplace(newBuffId, false);
    }
}
