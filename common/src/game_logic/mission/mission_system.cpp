#include "mission_system.h"

#include <ranges>

#include "muduo/base/Logging.h"

#include "src/game_config/condition_config.h"
#include "src/game_logic/constants/mission_constants.h"
#include "src/game_logic/comp/mission_comp.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/tips_id.h"

#include "component_proto/mission_comp.pb.h"
#include "event_proto/mission_event.pb.h"

std::array<std::function<bool(uint32_t, uint32_t)>, 5> function_compare({
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value >= conf_value; }},
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value > conf_value; }},
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value <= conf_value; }},
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value < conf_value; }},
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value == conf_value; }},
	});


uint32_t MissionSystem::GetReward(const GetRewardParam& param)
{
	if (!tls.registry.valid(param.player_))
	{
		LOG_ERROR << "player not found";
		return kInvalidParam;
	}
	auto* const try_mission_reward = tls.registry.try_get<MissionRewardPbComp>(param.player_);
	if (nullptr == try_mission_reward)
	{
		return kRetMissionPlayerMissionCompNotFound;
	}
	google::protobuf::Map<uint32_t, bool>* reward_mission_id = try_mission_reward->mutable_can_reward_mission_id();
	if (const auto mission_reward_it = reward_mission_id->find(param.mission_id_);
		mission_reward_it == reward_mission_id->end())
	{
		return kRetMissionGetRewardNoMissionId;
	}
	reward_mission_id->erase(param.mission_id_);
	return kRetOK;
}

uint32_t MissionSystem::Accept(const AcceptMissionEvent& accept_event)
{
	entt::entity player = entt::to_entity(accept_event.entity());
	auto* const try_mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == try_mission_comp)
	{
		return kRetMissionPlayerMissionCompNotFound;
	}
	//check
	RET_CHECK_RET(try_mission_comp->IsUnAccepted(accept_event.mission_id())) //已经接受过
	RET_CHECK_RET(try_mission_comp->IsUnCompleted(accept_event.mission_id())) //已经完成
	CheckCondition(!try_mission_comp->GetMissionConfig()->HasKey(accept_event.mission_id()), kRetTableId)

	auto mission_sub_type = try_mission_comp->GetMissionConfig()->GetMissionSubType(accept_event.mission_id());
	auto mission_type = try_mission_comp->GetMissionConfig()->GetMissionType(accept_event.mission_id());
	if (try_mission_comp->IsMissionTypeNotRepeated())
	{
		const UInt32PairSet::value_type mission_and_mission_subtype_pair(mission_type, mission_sub_type);
		CheckCondition(try_mission_comp->GetTypeFilter().find(mission_and_mission_subtype_pair)
			!= try_mission_comp->GetTypeFilter().end(), kRetMissionTypeRepeated)
	}
	MissionPbComp mission_pb;
	mission_pb.set_id(accept_event.mission_id());
	for (const auto& condition_ids = try_mission_comp->GetMissionConfig()->condition_id(accept_event.mission_id());
		const auto condition_id : condition_ids)
	{
		const auto* const condition_row = condition_config::GetSingleton().get(condition_id);
		if (nullptr == condition_row)
		{
			LOG_ERROR << "has not condition" << condition_id;
			continue;
		}
		//表的条件怎么改都无所谓,只有条件和表对应上就加进度
		mission_pb.add_progress(0);
		try_mission_comp->GetEventMissionsClassify()[condition_row->condition_type()].emplace(accept_event.mission_id());
	}
	try_mission_comp->GetMissionsComp().mutable_missions()->insert({ accept_event.mission_id(), std::move(mission_pb) });
	if (try_mission_comp->IsMissionTypeNotRepeated())
	{
		const UInt32PairSet::value_type mission_and_mission_subtype_pair(mission_type, mission_sub_type);
		try_mission_comp->GetTypeFilter().emplace(mission_and_mission_subtype_pair);
	}

	//todo
	{
		OnAcceptedMissionEvent on_accepted_mission_event;
		on_accepted_mission_event.set_entity(entt::to_integral(player));
		on_accepted_mission_event.set_mission_id(accept_event.mission_id());
		tls.dispatcher.enqueue(on_accepted_mission_event);
	}
	return kRetOK;
}

uint32_t MissionSystem::Abandon(const AbandonParam& param)
{
	auto* const try_mission_comp = tls.registry.try_get<MissionsComp>(param.player_);
	if (nullptr == try_mission_comp)
	{
		return kRetMissionPlayerMissionCompNotFound;
	}
	//已经完成
	RET_CHECK_RET(try_mission_comp->IsUnCompleted(param.mission_id_))
		auto* const try_mission_reward = tls.registry.try_get<MissionRewardPbComp>(param.player_);
	if (nullptr != try_mission_reward)
	{
		try_mission_reward->mutable_can_reward_mission_id()->erase(param.mission_id_);
	}
	try_mission_comp->GetMissionsComp().mutable_missions()->erase(param.mission_id_);
	try_mission_comp->GetMissionsComp().mutable_complete_missions()->erase(param.mission_id_);
	try_mission_comp->GetMissionsComp().mutable_mission_begin_time()->erase(param.mission_id_);
	DeleteMissionClassify(param.player_, param.mission_id_);
	return kRetOK;
}

void MissionSystem::CompleteAllMission(entt::entity player, uint32_t op)
{
	auto* const try_mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == try_mission_comp)
	{
		return;
	}
	for (const auto& key : try_mission_comp->GetMissionsComp().missions() | std::views::keys)
	{
		try_mission_comp->GetMissionsComp().mutable_complete_missions()->insert({ key, false });
	}
	try_mission_comp->GetMissionsComp().mutable_missions()->clear();
}

bool IsConditionCompleted(uint32_t condition_id, const uint32_t progress_value)
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


void MissionSystem::Receive(const MissionConditionEvent& condition_event)
{
	if (condition_event.condtion_ids().empty())
	{
		return;
	}
	entt::entity player = entt::to_entity(condition_event.entity());
	auto* const try_mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == try_mission_comp)
	{
		return;
	}
	const auto classify_mission_it = try_mission_comp->GetEventMissionsClassify().find(condition_event.type());
	if (classify_mission_it == try_mission_comp->GetEventMissionsClassify().end())
	{
		return;
	}
	UInt32Set temp_complete;
	//根据事件触发类型分类的任务
	//todo 同步异步事件
	for (const auto& classify_missions = classify_mission_it->second;
		auto lit : classify_missions)
	{
		auto mit = try_mission_comp->GetMissionsComp().mutable_missions()->find(lit);
		if (mit == try_mission_comp->GetMissionsComp().mutable_missions()->end())
		{
			continue;
		}
		auto& mission = mit->second;
		if (!UpdateMission(condition_event, mission))
		{
			continue;
		}
		const auto& conditions = try_mission_comp->GetMissionConfig()->condition_id(mission.id());
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
		try_mission_comp->GetMissionsComp().mutable_missions()->erase(mit);
	}

	OnMissionComplete(player, temp_complete);
}

void MissionSystem::DeleteMissionClassify(entt::entity player, uint32_t mission_id)
{
	auto* const try_mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == try_mission_comp)
	{
		return;
	}
	const auto& config_conditions = try_mission_comp->GetMissionConfig()->condition_id(mission_id);
	for (int32_t i = 0; i < config_conditions.size(); ++i)
	{
		const auto* const condition_row = condition_config::GetSingleton().get(config_conditions.Get(i));
		if (nullptr == condition_row)
		{
			continue;
		}
		try_mission_comp->GetEventMissionsClassify()[condition_row->condition_type()].erase(mission_id);
	}
	if (auto mission_sub_type = try_mission_comp->GetMissionConfig()->GetMissionSubType(mission_id);
		mission_sub_type > 0 &&
		try_mission_comp->IsMissionTypeNotRepeated())
	{
		const UInt32PairSet::value_type mission_and_mission_subtype_pair(try_mission_comp->GetMissionConfig()->GetMissionType(mission_id), mission_sub_type);
		try_mission_comp->GetTypeFilter().erase(mission_and_mission_subtype_pair);
	}
}

bool MissionSystem::UpdateMission(const MissionConditionEvent& condition_event,	MissionPbComp& mission)
{
	//todo 活跃度减少超
	if (condition_event.condtion_ids().empty())
	{
		return false;
	}
	const entt::entity player = entt::to_entity(condition_event.entity());
	const auto* const try_mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == try_mission_comp)
	{
		return false;
	}
	bool mission_updated = false;
	//如果我删除了某个条件，老玩家数据会不会错?正常任务是不能删除的，但是可以考虑删除条件
	const auto& mission_conditions = try_mission_comp->GetMissionConfig()->condition_id(mission.id());
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

void MissionSystem::OnMissionComplete(entt::entity player, const UInt32Set& completed_missions_this_time)
{
	if (completed_missions_this_time.empty())
	{
		return;
	}
	auto* const try_mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == try_mission_comp)
	{
		return;
	}
	for (const auto& mission_id : completed_missions_this_time)
	{
		DeleteMissionClassify(player, mission_id);
	}	
	//处理异步的
	auto* const mission_reward = tls.registry.try_get<MissionRewardPbComp>(player);
	MissionConditionEvent mission_condition_event;
	mission_condition_event.set_entity(entt::to_integral(player));
	mission_condition_event.set_type(kConditionCompleteMission);
	mission_condition_event.set_amount(1);
	for (const auto& mission_id : completed_missions_this_time)
	{
		try_mission_comp->GetMissionsComp().mutable_complete_missions()->insert({ mission_id, true });
		//自动领奖,给经验，为什么发事件？因为给经验升级了会马上接任务，或者触发一些任务的东西,
		//但是我需要不影响当前任务逻辑流程,也可以马上触发，看情况而定
		if (try_mission_comp->GetMissionConfig()->reward_id(mission_id) > 0 && try_mission_comp->GetMissionConfig()->auto_reward(mission_id))
		{
			OnMissionAwardEvent mission_award_event;
			mission_award_event.set_entity(entt::to_integral(player));
			mission_award_event.set_mission_id(mission_id);
			tls.dispatcher.enqueue(mission_award_event);
		}
		else if (nullptr != mission_reward && try_mission_comp->GetMissionConfig()->reward_id(mission_id) > 0)
		{
			//手动领奖
			mission_reward->mutable_can_reward_mission_id()->insert({ mission_id, false });
		}

		//todo 如果是活动不用走,让活动去接,这里应该是属于主任务系统的逻辑，想想怎么改方便，活动和任务逻辑分开，互不影响
		AcceptMissionEvent accept_mission_event;
		accept_mission_event.set_entity(entt::to_integral(player));
		const auto& next_missions = try_mission_comp->GetMissionConfig()->next_mission_id(mission_id);
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
