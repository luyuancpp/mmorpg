#include "buff_util.h"
#include <muduo/base/Logging.h>
#include "common_error_tip.pb.h"
#include "buff_config.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "thread_local/storage.h"
#include "buff_error_tip.pb.h"
#include "macros/return_define.h"
#include "thread_local/storage_game.h"

uint32_t BuffUtil::AddOrUpdateBuff(entt::entity parent, uint32_t buffTableId, const BuffAbilityContextPtrComp& abilityContext)
{
	auto [buffTable, result] = GetBuffTable(buffTableId);
	if (nullptr == buffTable)
	{
		return  result;
	}
	
	// 检查Buff是否可以被创建
	result = CanCreateBuff(parent, buffTableId);
	CHECK_RETURN_IF_NOT_OK(result);

	BuffComp newBuff;

	auto& buffList = tls.registry.get<BuffListComp>(parent).buffList;

	if (HandleExistingBuff(parent, buffTableId, abilityContext))
	{

		// 处理已存在的Buff，如更新层数
		// 如果处理成功，函数将返回，避免继续执行
		return kOK;
	}
	else
	{
		// 发出Buff觉醒事件
		bool shouldDestroy = OnBuffAwake(parent, buffTableId);

		if (!shouldDestroy)
		{
			// 生成唯一的Buff ID
			uint64_t newBuffId = 0;
			do
			{
				newBuffId = tlsGame.buffIdGenerator.Generate();
			} while (buffList.find(newBuffId) != buffList.end());

			newBuff.buffPB.set_buff_id( newBuffId);
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
	auto [tableBuff, result] = GetBuffTable(buffTableId);
	if (result != kOK) {
		return result;
	}

	auto& buffList = tls.registry.get<BuffListComp>(parent).buffList;

	bool buffExists = false;
	bool isImmune = false;

	for (const auto& [id, buff] : buffList) {
		auto [currentBuff, fetchResult] = GetBuffTable(id);
		if (fetchResult != kOK) {
			return fetchResult;
		}

		if (currentBuff->id() == tableBuff->id()) {
			// 已存在相同类型的Buff，检查层数
			if (buff.buffPB.layer() >= currentBuff->maxlayer()) {
				return kBuffMaxBuffStack;
			}
			buffExists = true;
		}

		if (currentBuff->immunetag() == tableBuff->tag()) {
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

void BuffUtil::OnAbilityExecuted(entt::entity parent)
{

}

void BuffUtil::OnBeforeGiveDamage(entt::entity parent)
{

}

void BuffUtil::OnAfterGiveDamage(entt::entity parent)
{

}

void BuffUtil::OnBeforeTakeDamage(entt::entity parent)
{

}

void BuffUtil::OnAfterTakeDamage(entt::entity parent)
{

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
