#include "mission.h"

#include <ranges>

#include "condition_config.h"
#include "constants/mission.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"

#include "proto/logic/component/mission_comp.pb.h"
#include "proto/logic/event/mission_event.pb.h"

MissionsComp::MissionsComp()
	: mission_config_(&MissionConfig::GetSingleton()),
	mission_type_not_repeated_(mission_config_->CheckTypeRepeated())
{
	// 初始化事件相关任务分类容器
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionTypeMax); ++i)
	{
		event_missions_classify_.emplace(i, UInt32Set{});
	}
}

std::size_t MissionsComp::CanGetRewardSize() const
{
	// 获取任务奖励组件
	const auto* const mission_reward = tls.registry.try_get<MissionRewardPbComp>(event_owner());
	if (nullptr == mission_reward)
	{
		return 0; // 若未找到奖励组件，则返回奖励数量为0
	}
	return static_cast<std::size_t>(mission_reward->can_reward_mission_id_size()); // 返回可领取奖励的任务数量
}

uint32_t MissionsComp::IsMissionUnaccepted(const uint32_t mission_id) const
{
	// 检查任务是否已接受
	if (missions_comp_.missions().find(mission_id) != missions_comp_.missions().end())
	{
		return kMissionIdRepeated; // 若任务已接受，则返回重复任务ID
	}
	return kOK; // 若任务未接受，则返回正常状态
}

uint32_t MissionsComp::IsMissionUncompleted(const uint32_t mission_id) const
{
	// 检查任务是否已完成
	if (missions_comp_.complete_missions().count(mission_id) > 0)
	{
		return kMissionAlreadyCompleted; // 若任务已完成，则返回已完成状态
	}
	return kOK; // 若任务未完成，则返回正常状态
}
