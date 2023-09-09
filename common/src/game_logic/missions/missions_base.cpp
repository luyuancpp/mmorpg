#include "missions_base.h"

#include <ranges>

#include "muduo/base/Logging.h"

#include "src/game_config/condition_config.h"
#include "src/game_logic/constants/mission_constants.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/tips_id.h"

#include "component_proto/mission_comp.pb.h"
#include "event_proto/mission_event.pb.h"

extern std::array<std::function<bool(uint32_t, uint32_t)>, 5> function_compare;

MissionsComp::MissionsComp()
	: mission_config_(&MissionConfig::GetSingleton()),
	  mission_type_not_repeated_(mission_config_->CheckTypeRepeated())
{
	for (uint32_t i = kConditionKillMonster; i < kConditionTypeMax; ++i)
	{
		event_missions_classify_.emplace(i, UInt32Set{});
	}
}

std::size_t MissionsComp::can_reward_size() const
{
	const auto* const try_mission_reward = tls.registry.try_get<MissionRewardPbComp>(event_owner());
	if (nullptr == try_mission_reward)
	{
		return 0;
	}
	return static_cast<std::size_t>(try_mission_reward->can_reward_mission_id_size());
}

bool MissionsComp::IsConditionCompleted(uint32_t condition_id, const uint32_t progress_value)
{
	const auto* p_condition_row = condition_config::GetSingleton().get(condition_id);
	if (nullptr == p_condition_row)
	{
		return false;
	}

	if (p_condition_row->operation() >= function_compare.size())
	{
		return function_compare[0](progress_value, p_condition_row->amount());
	}
	return function_compare.at(p_condition_row->operation())(progress_value, p_condition_row->amount());
}

uint32_t MissionsComp::IsUnAccepted(const uint32_t mission_id) const
{
	if (missions_comp_.missions().find(mission_id) != missions_comp_.missions().end())
	{
		return kRetMissionIdRepeated;
	}
	return kRetOK;
}

uint32_t MissionsComp::IsUnCompleted(const uint32_t mission_id) const
{
	if (missions_comp_.complete_missions().count(mission_id) > 0) //已经完成
	{
		return kRetMissionComplete;
	}
	return kRetOK;
}

void MissionsComp::Receive(const MissionConditionEvent& condition_event)
{
	if (condition_event.condtion_ids().empty())
	{
		return;
	}
	const auto classify_mission_it = event_missions_classify_.find(condition_event.type());
	if (classify_mission_it == event_missions_classify_.end())
	{
		return;
	}
	UInt32Set temp_complete;
	//根据事件触发类型分类的任务
	//todo 同步异步事件
	for (const auto& classify_missions = classify_mission_it->second;
		auto lit : classify_missions)
	{
		auto mit = missions_comp_.mutable_missions()->find(lit);
		if (mit == missions_comp_.mutable_missions()->end())
		{
			continue;
		}
		auto& mission = mit->second;
		if (!UpdateMission(condition_event, mission))
		{
			continue;
		}
		const auto& conditions = mission_config_->condition_id(mission.id());
		bool is_all_condition_complete = true;
		for (int32_t i = 0; i < mission.progress_size() && i < conditions.size(); ++i)
		{
			if (IsConditionCompleted(conditions[i], mission.progress(i)))
			{
				continue;
			}
			is_all_condition_complete = false;
			break;
		}
		if (!is_all_condition_complete)
		{
			continue;
		}
		mission.set_status(MissionPbComp::E_MISSION_COMPLETE);
		// to client
		temp_complete.emplace(mission.id());
		missions_comp_.mutable_missions()->erase(mit);
	}

	OnMissionComplete(temp_complete);
}

void MissionsComp::DeleteMissionClassify(uint32_t mission_id)
{
	const auto& config_conditions = mission_config_->condition_id(mission_id);
	for (int32_t i = 0; i < config_conditions.size(); ++i)
	{
		const auto* const condition_row = condition_config::GetSingleton().get(config_conditions.Get(i));
		if (nullptr == condition_row)
		{
			continue;
		}
		event_missions_classify_[condition_row->condition_type()].erase(mission_id);
	}
	if (auto mission_sub_type = mission_config_->mission_sub_type(mission_id);
		mission_sub_type > 0 &&
		mission_type_not_repeated_)
	{
		const UInt32PairSet::value_type mission_and_mission_subtype_pair(mission_config_->mission_type(mission_id), mission_sub_type);
		type_filter_.erase(mission_and_mission_subtype_pair);
	}
}

bool MissionsComp::UpdateMission(const MissionConditionEvent& condition_event,
                                                   MissionPbComp& mission) const
{
	//todo 活跃度减少超
	if (condition_event.condtion_ids().empty())
	{
		return false;
	}
	bool mission_updated = false;
	//如果我删除了某个条件，老玩家数据会不会错?正常任务是不能删除的，但是可以考虑删除条件
	const auto& mission_conditions = mission_config_->condition_id(mission.id());
	for (int32_t i = 0; i < mission.progress_size() && i < mission_conditions.size(); ++i)
	{
		const auto* const condition_row = condition_config::GetSingleton().get(mission_conditions.at(i));
		if (nullptr == condition_row)
		{
			continue;
		}
		const auto old_progress = mission.progress(i);
		if (IsConditionCompleted(condition_row->id(), old_progress))
		{
			continue;
		}
		if (condition_event.type() != condition_row->condition_type())
		{
			continue;
		}
		//表检测至少有一个condition
		std::size_t valid_config_condition_size = 0;
		std::size_t equal_condition_size = 0;
		auto calc_equal_condition_size = [&equal_condition_size, &condition_event, &valid_config_condition_size
			](const int32_t index, const auto& config_conditions)
		{
			if (config_conditions.size() > 0)
			{
				++valid_config_condition_size;
			}
			if (condition_event.condtion_ids().size() <= index)
			{
				return;
			}
			//验证条件和表里面的每列的多个条件是否有一项匹配
			for (int32_t ci = 0; ci < config_conditions.size(); ++ci)
			{
				if (condition_event.condtion_ids(index) != config_conditions.Get(ci))
				{
					continue;
				}
				//在这列中有一项匹配
				++equal_condition_size;
				break;
			}
		};
		calc_equal_condition_size(0, condition_row->condition1());
		calc_equal_condition_size(1, condition_row->condition2());
		calc_equal_condition_size(2, condition_row->condition3());
		calc_equal_condition_size(3, condition_row->condition4());
		//有效列中的条件列表都匹配了
		if (valid_config_condition_size == 0 || equal_condition_size != valid_config_condition_size)
		{
			continue;
		}
		mission_updated = true;
		mission.set_progress(i, condition_event.amount() + old_progress);
		auto new_progress = mission.progress(i);
		if (!IsConditionCompleted(condition_row->id(), new_progress))
		{
			continue;
		}
		// to client
		mission.set_progress(i, std::min(new_progress, condition_row->amount()));
		// to client
	}
	return mission_updated;
}

void MissionsComp::OnMissionComplete(const UInt32Set& completed_missions_this_time)
{
	if (completed_missions_this_time.empty())
	{
		return;
	}
	for (const auto& mission_id : completed_missions_this_time)
	{
		DeleteMissionClassify(mission_id);
	}
	//处理异步的
	auto* const try_mission_reward = tls.registry.try_get<MissionRewardPbComp>(event_owner());
	MissionConditionEvent mission_condition_event;
	mission_condition_event.set_entity(entt::to_integral(event_owner()));
	mission_condition_event.set_type(kConditionCompleteMission);
	mission_condition_event.set_amount(1);
	for (const auto& mission_id : completed_missions_this_time)
	{
		missions_comp_.mutable_complete_missions()->insert({mission_id, true});
		//自动领奖,给经验，为什么发事件？因为给经验升级了会马上接任务，或者触发一些任务的东西,
		//但是我需要不影响当前任务逻辑流程,也可以马上触发，看情况而定
		if (mission_config_->reward_id(mission_id) > 0 && mission_config_->auto_reward(mission_id))
		{
			OnMissionAwardEvent mission_award_event;
			mission_award_event.set_entity(entt::to_integral(event_owner()));
			mission_award_event.set_mission_id(mission_id);
			tls.dispatcher.enqueue(mission_award_event);
		}
		else if (nullptr != try_mission_reward && mission_config_->reward_id(mission_id) > 0)
		{
			//手动领奖
			try_mission_reward->mutable_can_reward_mission_id()->insert({mission_id, false});
		}

		//todo 如果是活动不用走,让活动去接,这里应该是属于主任务系统的逻辑，想想怎么改方便，活动和任务逻辑分开，互不影响
		AcceptMissionEvent accept_mission_event;
		accept_mission_event.set_entity(entt::to_integral(event_owner()));
		const auto& next_missions = mission_config_->next_mission_id(mission_id);
		for (int32_t i = 0; i < next_missions.size(); ++i)
		{
			accept_mission_event.set_mission_id(next_missions.Get(i));
			tls.dispatcher.enqueue(accept_mission_event);
		}
		mission_condition_event.clear_condtion_ids();
		mission_condition_event.mutable_condtion_ids()->Add(mission_id);
		tls.dispatcher.enqueue(mission_condition_event);
	}
}
