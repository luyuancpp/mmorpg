#include "missions_base.h"


#include "muduo/base/Logging.h"

#include "src/game_config/condition_config.h"
#include "src/game_logic/constants/mission_constants.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/missions/mission_system.h"
#include "src/game_logic/tips_id.h"
#include "src/util/random.h"

#include "event_proto/mission_event.pb.h"


static std::vector<std::function<bool(int32_t, int32_t)>> function_compare{
	{[](int32_t a, int32_t b) {return a >= b; }},
	{[](int32_t a, int32_t b) {return a > b; }},
	{[](int32_t a, int32_t b) {return a <= b; }},
	{[](int32_t a, int32_t b) {return a < b; }},
	{[](int32_t a, int32_t b) {return a == b; }},
};

MissionsComp::MissionsComp()
    : mission_config_(&MissionConfig::GetSingleton())
{
}

std::size_t MissionsComp::can_reward_size()
{
    auto try_mission_reward = registry.try_get<MissionRewardPbComp>(event_owner());
    if (nullptr == try_mission_reward)
    {
        return 0;
    }
    return try_mission_reward->can_reward_mission_id_size();
}

void MissionsComp::Init()
{
	for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_MAX; ++i)
	{
		event_missions_classify_.emplace(i, UInt32Set{});
	}
	if (mission_config_->CheckTypeRepeated())
	{
		registry.emplace<CheckTypeRepeatd>(event_owner());
	}
    auto try_dispatcher = registry.try_get<entt::dispatcher>(event_owner());
    if (nullptr != try_dispatcher)
    {
        try_dispatcher->sink<AcceptMissionEvent>().connect<&MissionEventReceiver::Receive1>();
        try_dispatcher->sink<MissionConditionEvent>().connect<&MissionEventReceiver::Receive2>();
        try_dispatcher->sink<OnAcceptedMissionEvent>().connect<&MissionEventReceiver::Receive3>();
    }
}

bool MissionsComp::IsConditionCompleted(uint32_t condition_id, uint32_t progress_value)
{
	auto p = condition_config::GetSingleton().get(condition_id);
	if (nullptr == p)
	{
        return false;
	}
	std::size_t operator_id = std::size_t(p->operation());
	if (!(operator_id >= 0 && operator_id < function_compare.size()))
	{
		operator_id = 0;
	}
    return function_compare[operator_id](progress_value, p->amount());
}

uint32_t MissionsComp::IsDoNotAccepted(uint32_t mission_id)const
{
	if (missions_comp_pb_.missions().count(mission_id) > 0)//�Ѿ����ܹ�
	{
		return kRetMissionIdRepeated;
	}
    return kRetOK;
}

uint32_t MissionsComp::IsDoNotCompleted(uint32_t mission_id)const
{
	if (missions_comp_pb_.complete_missions().count(mission_id) > 0)//�Ѿ����
	{
		return kRetMissionComplete;
	}
    return kRetOK;
}

uint32_t MissionsComp::GetReward(uint32_t missin_id)
{
	auto try_mission_reward = registry.try_get<MissionRewardPbComp>(event_owner());
	if (nullptr == try_mission_reward)
	{
		return kRetMissionPlayerMissionCompNotFound;
	}
    auto rmid = try_mission_reward->mutable_can_reward_mission_id();
    auto it = try_mission_reward->mutable_can_reward_mission_id()->find(missin_id);
    if (it == rmid->end())
    {
        return kRetMissionGetRewardNoMissionId;
    }
    rmid->erase(missin_id);
    return kRetOK;
}

uint32_t MissionsComp::Accept(const AcceptMissionEvent& accept_event)
{
    //check 
    RET_CHECK_RET(IsDoNotAccepted(accept_event.mission_id()));//�Ѿ����ܹ�
    RET_CHECK_RET(IsDoNotCompleted(accept_event.mission_id()));//�Ѿ����
    CheckCondtion(!mission_config_->HasKey(accept_event.mission_id()), kRetTableId);

    auto mission_sub_type = mission_config_->mission_sub_type(accept_event.mission_id());
    auto mission_type = mission_config_->mission_type(accept_event.mission_id());
    bool check_type_repeated =  mission_sub_type > 0 && registry.any_of<CheckTypeRepeatd>(event_owner());
    if (check_type_repeated)
    {
        UInt32PairSet::value_type p(mission_type, mission_sub_type);
        CheckCondtion(type_filter_.find(p) != type_filter_.end(), kRetMisionTypeRepeated);
    }
    MissionPbComp misison;
    misison.set_id(accept_event.mission_id());
    const auto& conditionids = mission_config_->condition_id(accept_event.mission_id());
    for (int32_t i = 0; i < conditionids.size(); ++i)
    {
        auto cid = conditionids[i];
        auto p = condition_config::GetSingleton().get(cid);
        if (nullptr == p)
        {
            LOG_ERROR << "has not condtion" << cid;
            continue;
        }
        //���������ô�Ķ�����ν,ֻ�������ͱ��Ӧ�Ͼͼӽ���
        misison.add_progress(0);
        event_missions_classify_[p->condition_type()].emplace(accept_event.mission_id());
    }
    missions_comp_pb_.mutable_missions()->insert({ accept_event.mission_id(), std::move(misison) });
    if (check_type_repeated)
    {
        UInt32PairSet::value_type p(mission_type, mission_sub_type);
        type_filter_.emplace(p);
    }

    //todo 
    auto try_dispatcher = registry.try_get<entt::dispatcher>(event_owner());
    if (nullptr != try_dispatcher)
    {
        OnAcceptedMissionEvent on_accepted_mission_event;
        on_accepted_mission_event.set_entity(entt::to_integral(event_owner()));
        on_accepted_mission_event.set_mission_id(accept_event.mission_id());
		try_dispatcher->enqueue(on_accepted_mission_event);
    }
    return kRetOK;
}

uint32_t MissionsComp::Abandon(uint32_t mission_id)
{
    RET_CHECK_RET(IsDoNotCompleted(mission_id));//�Ѿ����
	auto try_mission_reward = registry.try_get<MissionRewardPbComp>(event_owner());
	if (nullptr != try_mission_reward)
	{
        try_mission_reward->mutable_can_reward_mission_id()->erase(mission_id);
	}
    missions_comp_pb_.mutable_missions()->erase(mission_id);
    missions_comp_pb_.mutable_complete_missions()->erase(mission_id);
    missions_comp_pb_.mutable_mission_begin_time()->erase(mission_id);
    DelMissionClassify(mission_id);
    return kRetOK;
}

void MissionsComp::CompleteAllMission()
{
    for (auto& meit : missions_comp_pb_.missions())
    {
        missions_comp_pb_.mutable_complete_missions()->insert({ meit.first, false });
    }
    missions_comp_pb_.mutable_missions()->clear();
}

void MissionsComp::Receive(const MissionConditionEvent& condition_event)
{
    if (condition_event.condtion_ids().empty())
    {
        return;
    }
    auto it =  event_missions_classify_.find(condition_event.type());
    if (it ==  event_missions_classify_.end())
    {
        return;
    }
    auto missions = missions_comp_pb_.mutable_missions();
    UInt32Set temp_complete;
    auto& classify_missions = it->second;//�����¼��������ͷ��������
    for (auto lit : classify_missions)
    {
        auto mit = missions->find(lit);
        if (mit == missions->end())
        {
            continue;
        }
        auto& mission = mit->second;
        if (!UpdateMissionByCompareCondition(condition_event, mission))
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
            break;
        }
        mission.set_status(MissionPbComp::E_MISSION_COMPLETE);
        // to client
        temp_complete.emplace(mission.id());
        missions->erase(mit);
    }

    OnMissionComplete(temp_complete);
}

void MissionsComp::DelMissionClassify(uint32_t mission_id)
{
    auto& config_conditions = mission_config_->condition_id(mission_id);
    for (int32_t i = 0; i < config_conditions.size(); ++i)
    {
        auto condition_row = condition_config::GetSingleton().get(config_conditions.Get(i));
        if (nullptr == condition_row)
        {
            continue;
        }
        event_missions_classify_[condition_row->condition_type()].erase(mission_id);
    }
    auto mission_sub_type = mission_config_->mission_sub_type(mission_id);
    bool check_type_repeated = mission_sub_type > 0 && registry.any_of<CheckTypeRepeatd>(event_owner());
    if (check_type_repeated)
    {
		UInt32PairSet::value_type p(mission_config_->mission_type(mission_id), mission_sub_type);
		type_filter_.erase(p);
    }
}

bool MissionsComp::UpdateMissionByCompareCondition(const MissionConditionEvent& condition_event, MissionPbComp& mission)
{
    if (condition_event.condtion_ids().empty())
    {
        return false;
    }
    bool mission_updated = false;
    //�����ɾ����ĳ����������������ݻ᲻���?���������ǲ���ɾ���ģ����ǿ��Կ���ɾ������
    auto& mission_conditions = mission_config_->condition_id(mission.id());
    for (int32_t i = 0; i < mission.progress_size() && i < mission_conditions.size(); ++i)
    {
        auto condition_row = condition_config::GetSingleton().get(mission_conditions.at(i));
        if (nullptr == condition_row)
        {
            continue;
        }
        auto old_progress = mission.progress(i);
		if (IsConditionCompleted(condition_row->id(), old_progress))
		{
			continue;
		}
        if (condition_event.type() != condition_row->condition_type())
        {
            continue;
        }
        //����������һ��condition
        std::size_t config_condition_size = 0;
        std::size_t equal_condition_size = 0;
        auto calc_equal_condition_size = [&equal_condition_size, &condition_event, &config_condition_size](auto index, const auto& config_conditions)
        {
			if (config_conditions.size() > 0)
			{
				++config_condition_size;
			}
            if (condition_event.condtion_ids().size() <= index)
            {
                return;
            }           
			for (int32_t ci = 0; ci < config_conditions.size(); ++ci)
			{
				if (condition_event.condtion_ids(index) != config_conditions.Get(ci))
				{
					continue;
				}
				++equal_condition_size;
				break;
			}
        };
        calc_equal_condition_size(0, condition_row->condition1());
        calc_equal_condition_size(1, condition_row->condition2());
        calc_equal_condition_size(2, condition_row->condition3());
        calc_equal_condition_size(3, condition_row->condition4());
        if (config_condition_size == 0 || equal_condition_size != config_condition_size)
        {
            continue;
        }
        mission_updated = true;
        mission.set_progress(i , condition_event.amount() + old_progress);
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
    auto try_mission_reward = registry.try_get<MissionRewardPbComp>(event_owner());
    auto try_dispatcher = registry.try_get<entt::dispatcher>(event_owner());
    for (auto& mission_id : completed_missions_this_time)
    {
        missions_comp_pb_.mutable_complete_missions()->insert({ mission_id, true });
        if (nullptr != try_mission_reward && mission_config_->reward_id(mission_id) > 0)
        {
            try_mission_reward->mutable_can_reward_mission_id()->insert({ mission_id, false });
        }
        // auto reward
        DelMissionClassify(mission_id);

        //����ǻ������
		AcceptMissionEvent accept_mission_event;
		accept_mission_event.set_entity(entt::to_integral(event_owner()));
        auto& next_missions = mission_config_->next_mission_id(mission_id);
        for (int32_t i = 0; i < next_missions.size(); ++i)
        {
			if (nullptr == try_dispatcher)
			{
                continue;
			}			
			accept_mission_event.set_mission_id(next_missions.Get(i));
			try_dispatcher->enqueue(accept_mission_event);
        }
    }
    TriggerMissionCompleteCondition(completed_missions_this_time);
}

void MissionsComp::TriggerMissionCompleteCondition(const UInt32Set& temp_complete)
{
    auto try_dispatcher = registry.try_get<entt::dispatcher>(event_owner());
	if (nullptr == try_dispatcher)
	{
        return; 	
	}
	MissionConditionEvent mission_condition_event;
	mission_condition_event.set_entity(entt::to_integral(event_owner()));
	mission_condition_event.set_type(E_CONDITION_COMPLELTE_MISSION);
	mission_condition_event.set_amount(1);
	for (auto& it : temp_complete)
	{
		mission_condition_event.clear_condtion_ids();
		mission_condition_event.mutable_condtion_ids()->Add(it);
		try_dispatcher->enqueue(mission_condition_event);
	}
}
