#include "mission_system.h"
#include <ranges>
#include "muduo/base/Logging.h"
#include "condition_config.h"
#include "constants/mission.h"
#include "comp/mission.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"
#include "proto/logic/component/mission_comp.pb.h"
#include "proto/logic/event/mission_event.pb.h"

// 定义5种比较函数，用于任务条件的比较
std::array<std::function<bool(uint32_t, uint32_t)>, 5> function_compare({
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value >= conf_value; }},
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value > conf_value; }},
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value <= conf_value; }},
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value < conf_value; }},
	{[](const uint32_t real_value, const uint32_t conf_value) { return real_value == conf_value; }},
	});

// 获取任务奖励
uint32_t MissionSystem::GetReward(const GetRewardParam& param) {
	if (!tls.registry.valid(param.player_)) {
		LOG_ERROR << "player not found";
		return kInvalidParam;
	}
	auto* const mission_reward = tls.registry.try_get<MissionRewardPbComp>(param.player_);
	if (nullptr == mission_reward) {
		return kRetMissionPlayerMissionCompNotFound;
	}
	google::protobuf::Map<uint32_t, bool>* reward_mission_id = mission_reward->mutable_can_reward_mission_id();
	if (const auto mission_reward_it = reward_mission_id->find(param.mission_id_);
		mission_reward_it == reward_mission_id->end()) {
		return kRetMissionGetRewardNoMissionId;
	}
	reward_mission_id->erase(param.mission_id_);
	return kRetOK;
}

// 检查接受任务的条件
uint32_t MissionSystem::CheckAcceptConditions(const AcceptMissionEvent& accept_event, MissionsComp* mission_comp) {
	RET_CHECK_RET(mission_comp->IsUnAccepted(accept_event.mission_id()))
	RET_CHECK_RET(mission_comp->IsUnCompleted(accept_event.mission_id()))
	CHECK_CONDITION(!mission_comp->GetMissionConfig()->HasKey(accept_event.mission_id()), kRetTableId)

	auto mission_sub_type = mission_comp->GetMissionConfig()->GetMissionSubType(accept_event.mission_id());
	auto mission_type = mission_comp->GetMissionConfig()->GetMissionType(accept_event.mission_id());

	if (mission_comp->IsMissionTypeNotRepeated()) {
		const UInt32PairSet::value_type mission_and_mission_subtype_pair(mission_type, mission_sub_type);
		CHECK_CONDITION(mission_comp->GetTypeFilter().find(mission_and_mission_subtype_pair) != mission_comp->GetTypeFilter().end(), kRetMissionTypeRepeated)
	}

	return kRetOK;
}

// 接受任务
uint32_t MissionSystem::Accept(const AcceptMissionEvent& accept_event) {
	const entt::entity player = entt::to_entity(accept_event.entity());
	auto* const mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == mission_comp) {
		return kRetMissionPlayerMissionCompNotFound;
	}

	auto ret = CheckAcceptConditions(accept_event, mission_comp);
	if (ret != kRetOK) {
		return ret;
	}

	auto mission_sub_type = mission_comp->GetMissionConfig()->GetMissionSubType(accept_event.mission_id());
	auto mission_type = mission_comp->GetMissionConfig()->GetMissionType(accept_event.mission_id());
	if (mission_comp->IsMissionTypeNotRepeated()) {
		const UInt32PairSet::value_type mission_and_mission_subtype_pair(mission_type, mission_sub_type);
		mission_comp->GetTypeFilter().emplace(mission_and_mission_subtype_pair);
	}

	MissionPbComp mission_pb;
	mission_pb.set_id(accept_event.mission_id());
	for (const auto& condition_id : mission_comp->GetMissionConfig()->condition_id(accept_event.mission_id())) {
		const auto* const condition_row = condition_config::GetSingleton().get(condition_id);
		if (nullptr == condition_row) {
			LOG_ERROR << "has not condition" << condition_id;
			continue;
		}
		mission_pb.add_progress(0);
		mission_comp->GetEventMissionsClassify()[condition_row->condition_type()].emplace(accept_event.mission_id());
	}
	mission_comp->GetMissionsComp().mutable_missions()->insert({ accept_event.mission_id(), std::move(mission_pb) });

	{
		OnAcceptedMissionEvent on_accepted_mission_event;
		on_accepted_mission_event.set_entity(entt::to_integral(player));
		on_accepted_mission_event.set_mission_id(accept_event.mission_id());
		tls.dispatcher.enqueue(on_accepted_mission_event);
	}
	return kRetOK;
}

// 放弃任务
uint32_t MissionSystem::Abandon(const AbandonParam& param) {
	auto* const mission_comp = tls.registry.try_get<MissionsComp>(param.player_);
	if (nullptr == mission_comp) {
		return kRetMissionPlayerMissionCompNotFound;
	}

	RET_CHECK_RET(mission_comp->IsUnCompleted(param.mission_id_))

		auto* const mission_reward = tls.registry.try_get<MissionRewardPbComp>(param.player_);
	if (nullptr != mission_reward) {
		mission_reward->mutable_can_reward_mission_id()->erase(param.mission_id_);
	}

	mission_comp->GetMissionsComp().mutable_missions()->erase(param.mission_id_);
	mission_comp->GetMissionsComp().mutable_complete_missions()->erase(param.mission_id_);
	mission_comp->GetMissionsComp().mutable_mission_begin_time()->erase(param.mission_id_);

	DeleteMissionClassify(param.player_, param.mission_id_);
	return kRetOK;
}

// 完成所有任务
void MissionSystem::CompleteAllMission(entt::entity player, uint32_t op) {
	auto* const mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == mission_comp) {
		return;
	}

	for (const auto& key : mission_comp->GetMissionsComp().missions() | std::views::keys) {
		mission_comp->GetMissionsComp().mutable_complete_missions()->insert({ key, false });
	}

	mission_comp->GetMissionsComp().mutable_missions()->clear();
}

// 检查条件是否已完成
bool IsConditionCompleted(uint32_t condition_id, const uint32_t progress_value) {
	const auto* p_condition_row = condition_config::GetSingleton().get(condition_id);
	if (nullptr == p_condition_row) {
		return false;
	}

	if (p_condition_row->operation() >= function_compare.size()) {
		return function_compare[0](progress_value, p_condition_row->amount());
	}
	return function_compare.at(p_condition_row->operation())(progress_value, p_condition_row->amount());
}

// 处理任务条件事件
void MissionSystem::Receive(const MissionConditionEvent& condition_event) {
	if (condition_event.condtion_ids().empty()) {
		return;
	}

	const entt::entity player = entt::to_entity(condition_event.entity());
	auto* const mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == mission_comp) {
		return;
	}

	const auto classify_mission_it = mission_comp->GetEventMissionsClassify().find(condition_event.type());
	if (classify_mission_it == mission_comp->GetEventMissionsClassify().end()) {
		return;
	}

	UInt32Set temp_complete;
	for (auto lit : classify_mission_it->second) {
		auto mit = mission_comp->GetMissionsComp().mutable_missions()->find(lit);
		if (mit == mission_comp->GetMissionsComp().mutable_missions()->end()) {
			continue;
		}
		auto& mission = mit->second;

		if (!UpdateMission(condition_event, mission)) {
			continue;
		}

		const auto& conditions = mission_comp->GetMissionConfig()->condition_id(mission.id());
		bool is_all_condition_complete = true;
		for (int32_t i = 0; i < mission.progress_size() && i < conditions.size(); ++i) {
			if (IsConditionCompleted(conditions[i], mission.progress(i))) {
				continue;
			}
			is_all_condition_complete = false;
			break;
		}

		if (!is_all_condition_complete) {
			continue;
		}

		mission.set_status(MissionPbComp::E_MISSION_COMPLETE);
		temp_complete.emplace(mission.id());
		mission_comp->GetMissionsComp().mutable_missions()->erase(mit);
	}

	OnMissionComplete(player, temp_complete);
}

// 移除任务分类
void MissionSystem::RemoveMissionFromClassify(MissionsComp* mission_comp, uint32_t mission_id) {
	const auto& config_conditions = mission_comp->GetMissionConfig()->condition_id(mission_id);
	for (int32_t i =

		0; i < config_conditions.size(); ++i) {
		const auto* const condition_row = condition_config::GetSingleton().get(config_conditions.Get(i));
		if (nullptr == condition_row) {
			continue;
		}
		mission_comp->GetEventMissionsClassify()[condition_row->condition_type()].erase(mission_id);
	}
}

// 删除任务分类
void MissionSystem::DeleteMissionClassify(entt::entity player, uint32_t mission_id) {
	auto* const mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == mission_comp) {
		return;
	}

	RemoveMissionFromClassify(mission_comp, mission_id);

	if (auto mission_sub_type = mission_comp->GetMissionConfig()->GetMissionSubType(mission_id);
		mission_sub_type > 0 && mission_comp->IsMissionTypeNotRepeated()) {
		const UInt32PairSet::value_type mission_and_mission_subtype_pair(mission_comp->GetMissionConfig()->GetMissionType(mission_id), mission_sub_type);
		mission_comp->GetTypeFilter().erase(mission_and_mission_subtype_pair);
	}
}

// 更新任务进度
bool MissionSystem::UpdateMission(const MissionConditionEvent& condition_event, MissionPbComp& mission) {
	if (condition_event.condtion_ids().empty()) {
		return false;
	}

	const entt::entity player = entt::to_entity(condition_event.entity());
	const auto* const mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == mission_comp) {
		return false;
	}

	bool mission_updated = false;
	const auto& mission_conditions = mission_comp->GetMissionConfig()->condition_id(mission.id());

	for (int32_t i = 0; i < mission.progress_size() && i < mission_conditions.size(); ++i) {
		const auto* const condition_row = condition_config::GetSingleton().get(mission_conditions.at(i));
		if (nullptr == condition_row) {
			continue;
		}

		if (UpdateMissionProgress(condition_event, mission, i, condition_row)) {
			mission_updated = true;
		}
	}

	if (mission_updated) {
		UpdateMissionStatus(mission, mission_conditions);
	}

	return mission_updated;
}

// 更新任务进度
bool MissionSystem::UpdateMissionProgress(const MissionConditionEvent& condition_event, MissionPbComp& mission, int index, const condition_row* const condition_row) {
	const auto old_progress = mission.progress(index);

	if (IsConditionCompleted(condition_row->id(), old_progress)) {
		return false;
	}

	if (condition_event.type() != condition_row->condition_type()) {
		return false;
	}

	size_t config_condition_size = 0;
	size_t match_condition_size = 0;

	auto calc_match_condition_size = [&match_condition_size, &condition_event, &config_condition_size](const auto& config_conditions, size_t index) {
		if (config_conditions.size() > index) {
			++config_condition_size;
			if (std::find(condition_event.condtion_ids().begin(), condition_event.condtion_ids().end(), config_conditions.Get(index)) != condition_event.condtion_ids().end()) {
				++match_condition_size;
			}
		}
		};

	calc_match_condition_size(condition_row->condition1(), 0);
	calc_match_condition_size(condition_row->condition2(), 1);
	calc_match_condition_size(condition_row->condition3(), 2);
	calc_match_condition_size(condition_row->condition4(), 3);

	if (config_condition_size > 0 && match_condition_size != config_condition_size) {
		return false;
	}

	mission.set_progress(index, condition_event.amount() + old_progress);

	return true;
}

// 更新任务状态
void MissionSystem::UpdateMissionStatus(MissionPbComp& mission, const google::protobuf::RepeatedField<uint32_t>& mission_conditions) {
	for (int32_t i = 0; i < mission.progress_size() && i < mission_conditions.size(); ++i) {
		const auto* const condition_row = condition_config::GetSingleton().get(mission_conditions.at(i));
		if (nullptr == condition_row) {
			continue;
		}

		auto new_progress = mission.progress(i);

		if (!IsConditionCompleted(condition_row->id(), new_progress)) {
			continue;
		}

		mission.set_progress(i, std::min(new_progress, condition_row->amount()));
	}
}

// 处理任务完成事件
void MissionSystem::OnMissionComplete(entt::entity player, const UInt32Set& completed_missions_this_time) {
	if (completed_missions_this_time.empty()) {
		return;
	}

	auto* const mission_comp = tls.registry.try_get<MissionsComp>(player);
	if (nullptr == mission_comp) {
		return;
	}

	for (const auto& mission_id : completed_missions_this_time) {
		DeleteMissionClassify(player, mission_id);
	}

	auto* const mission_reward = tls.registry.try_get<MissionRewardPbComp>(player);
	MissionConditionEvent mission_condition_event;
	mission_condition_event.set_entity(entt::to_integral(player));
	mission_condition_event.set_type(static_cast<uint32_t>(eCondtionType::kConditionCompleteMission));
	mission_condition_event.set_amount(1);

	for (const auto& mission_id : completed_missions_this_time) {
		mission_comp->GetMissionsComp().mutable_complete_missions()->insert({ mission_id, true });

		if (mission_comp->GetMissionConfig()->reward_id(mission_id) > 0 && mission_comp->GetMissionConfig()->auto_reward(mission_id)) {
			OnMissionAwardEvent mission_award_event;
			mission_award_event.set_entity(entt::to_integral(player));
			mission_award_event.set_mission_id(mission_id);
			tls.dispatcher.enqueue(mission_award_event);
		}
		else if (nullptr != mission_reward && mission_comp->GetMissionConfig()->reward_id(mission_id) > 0) {
			mission_reward->mutable_can_reward_mission_id()->insert({ mission_id, false });
		}

		AcceptMissionEvent accept_mission_event;
		accept_mission_event.set_entity(entt::to_integral(player));
		const auto& next_missions = mission_comp->GetMissionConfig()->next_mission_id(mission_id);

		for (int32_t i = 0; i < next_missions.size(); ++i) {
			accept_mission_event.set_mission_id(next_missions.Get(i));
			tls.dispatcher.enqueue(accept_mission_event);
		}

		mission_condition_event.clear_condtion_ids();
		mission_condition_event.mutable_condtion_ids()->Add(mission_id);
		tls.dispatcher.enqueue(mission_condition_event);
	}
}
