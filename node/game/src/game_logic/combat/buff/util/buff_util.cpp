#include "buff_util.h"
#include <muduo/base/Logging.h>
#include "common_error_tip.pb.h"
#include "buff_config.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "thread_local/storage.h"
#include "buff_error_tip.pb.h"
#include "macros/return_define.h"
#include "util/node_id_generator.h"

using NodeIdGenerator32BitId = NodeIdGenerator<uint64_t, 32>;

uint32_t BuffUtil::CreatedBuff(entt::entity parent, uint32_t buffTableId, const BuffAbilityContextPtrComp& AbilityContext)
{
	// 检查Buff是否可以被创建
	uint32_t result = CheckIfBuffCanBeCreated(parent, buffTableId);
	CHECK_RETURN_IF_NOT_OK(result);

	BuffComp buff;

	auto& buffListComp = tls.registry.get<BuffListComp>(parent);
	auto& buffList = buffListComp.buffList;

	// 查找并增加层数
	if (auto buffIt = buffList.find(buffTableId); buffIt != buffList.end())
	{
		buffIt->second.pb.set_layer(buffIt->second.pb.layer() + 1);
	}
	else
	{
		// 发出Buff觉醒事件
		auto destroy = OnBuffAwake(parent, buffTableId);

		if (!destroy)
		{
			buffList[buffTableId] = buff;
		}

		OnBuffStart(parent, buffTableId);
	}

	return kOK;
}

uint32_t BuffUtil::CheckIfBuffCanBeCreated(entt::entity parent, uint32_t buffTableId) {
	auto [tableBuff, result] = GetBuffTable(buffTableId);
	if (result != kOK) {
		return result;
	}

	auto& buffListComp = tls.registry.get<BuffListComp>(parent);
	const auto& buffList = buffListComp.buffList;

	bool buffExists = false;
	bool isImmune = false;

	for (const auto& [id, buff] : buffList) {
		auto [currentBuff, fetchResult] = GetBuffTable(id);
		if (fetchResult != kOK) {
			return fetchResult;
		}

		if (currentBuff->id() == tableBuff->id()) {
			// 已存在相同类型的Buff，检查层数
			if (buff.pb.layer() >= currentBuff->maxlayer()) {
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


bool BuffUtil::OnBuffAwake(entt::entity parent, uint32_t buffTableId)
{
	return false;
}

void BuffUtil::OnBuffStart(entt::entity parent, uint32_t buffTableId)
{

}

void BuffUtil::OnBuffRefresh()
{

}

void BuffUtil::OnBuffRemove()
{

}

void BuffUtil::OnBuffDestroy()
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
