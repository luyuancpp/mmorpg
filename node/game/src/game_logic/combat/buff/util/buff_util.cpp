#include "buff_util.h"
#include <muduo/base/Logging.h>
#include "common_error_tip.pb.h"
#include "buff_config.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "thread_local/storage.h"
#include "buff_error_tip.pb.h"
#include "macros/return_define.h"
#include "thread_local/storage_game.h"
#include "event/buff_event.pb.h"

void BuffUtil::Initialize()
{
	tls.dispatcher.sink<AbilityExecutedEvent>().connect<&BuffUtil::OnAbilityExecuted>();
}

uint32_t BuffUtil::AddOrUpdateBuff(entt::entity parent, uint32_t buffTableId, const BuffAbilityContextPtrComp& abilityContext){
	auto [buffTable, result] = GetBuffTable(buffTableId);
	if (nullptr == buffTable){
		return  result;
	}
	
	// 检查Buff是否可以被创建
	result = CanCreateBuff(parent, buffTableId);
	CHECK_RETURN_IF_NOT_OK(result);

	
	auto& buffList = tls.registry.get<BuffListComp>(parent).buffList;

	if (HandleExistingBuff(parent, buffTableId, abilityContext)){
		return kOK;
	}
	else{
		BuffComp newBuff;

		if (buffTable->nocaster()) {
			newBuff.buffPB.set_caster(entt::null);
		}else
		{
			newBuff.buffPB.set_caster(abilityContext->caster);
		}
		
		// 发出Buff觉醒事件
		bool shouldDestroy = OnBuffAwake(parent, buffTableId);

		if (!shouldDestroy)
		{
			// 生成唯一的Buff ID
			uint64_t newBuffId = 0;
			do
			{
				newBuffId = tlsGame.buffIdGenerator.Generate();
			} while (buffList.contains(newBuffId));

			newBuff.buffPB.set_buff_id( newBuffId);
			newBuff.abilityContext = abilityContext;
			
			buffList[newBuffId] = newBuff;
		}

		OnBuffStart(parent, buffTableId);
	}

	return kOK;
}

void BuffUtil::OnBuffExpire(entt::entity parent, uint64_t buffId)
{
	auto& buffList = tls.registry.get<BuffListComp>(parent).buffList;

	OnBuffRemove(parent, buffId);

	buffList.erase(buffId);

	OnBuffDestroy(parent);
}

uint32_t BuffUtil::CanCreateBuff(entt::entity parent, uint32_t buffTableId) {
	auto [buffTable, result] = GetBuffTable(buffTableId);
	if (result != kOK) {
		return result;
	}

	auto& buffList = tls.registry.get<BuffListComp>(parent).buffList;

	bool buffExists = false;
	bool isImmune = false;

	for (const auto& [id, buff] : buffList) {
		auto [currentBuffTable, fetchResult] = GetBuffTable(id);
		if (fetchResult != kOK) {
			return fetchResult;
		}

		if (currentBuffTable->id() == buffTable->id()) {
			// 已存在相同类型的Buff，检查层数
			if (buff.buffPB.layer() >= currentBuffTable->maxlayer()) {
				return kBuffMaxBuffStack;
			}
			buffExists = true;
		}

		if (currentBuffTable->immunetag() == buffTable->tag()) {
			isImmune = true;
		}
	}

	if (isImmune) {
		return kBuffTargetImmuneToBuff;
	}

	// 其他检查条件
	// ...

	return kOK; // 可以创建
}

bool BuffUtil::HandleExistingBuff(entt::entity parent, uint32_t buffTableId, const BuffAbilityContextPtrComp& abilityContext)
{
	auto& buffList = tls.registry.get<BuffListComp>(parent).buffList;

	for (auto& [buffId, buffPB] : buffList)
	{
		if (buffPB.buffPB.buff_table_id() == buffTableId &&
			buffPB.abilityContext->caster == abilityContext->caster)
		{
			OnBuffRefresh(parent, buffTableId, abilityContext, buffPB);
			return true;
		}
	}
	return false;
}

bool BuffUtil::OnBuffAwake(entt::entity parent, uint32_t buffTableId)
{
	return false;
}

void BuffUtil::OnBuffStart(entt::entity parent, uint32_t buffTableId)
{

}

void BuffUtil::OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const BuffAbilityContextPtrComp& abilityContext, BuffComp& buffComp)
{

}

void BuffUtil::OnBuffRemove(entt::entity parent, uint64_t buffId)
{

}

void BuffUtil::OnBuffDestroy(entt::entity parent)
{

}

void BuffUtil::StartIntervalThink()
{

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

void BuffUtil::OnAbilityExecuted(AbilityExecutedEvent& event)
{

}

void BuffUtil::OnBeforeGiveDamage(entt::entity parent)
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
	//	for (auto& buff : tls.registry.get<BuffListComp>(event.target).buffList) {
	//		if (buff.second.HasFlag(DamageFlag_NotMiss)) {
	//			event.damageFlags |= DamageFlag_NotMiss;
	//		}
	//	}
}

void BuffUtil::OnAfterGiveDamage(entt::entity parent)
{
	// 检查并应用DOT效果
	//for (auto& buff : tls.registry.get<BuffListComp>(event.target).buffList) {
	//	if (buff.second.HasChanceForDOT()) {
	//		// 添加DOT Buff
	//		AddDOTBuff(event.target);
	//	}
	//}
}

void BuffUtil::OnBeforeTakeDamage(entt::entity parent)
{
	//auto& buffs = tls.registry.get<BuffListComp>(event.target).buffList;
	//for (auto& [buffId, buff] : buffs) {
	//	if (buff.HasShield()) {
	//		// 假设护盾Buff会减少伤害
	//		event.damageAmount *= 0.5f; // 示例：护盾减少50%伤害
	//	}
	//}
}

void BuffUtil::OnAfterTakeDamage(entt::entity parent)
{
	// 检查并应用额外效果
	//auto& buffs = tls.registry.get<BuffListComp>(event.target).buffList;
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
