#include "missions.h"

#include "src/game_logic/game_registry.h"
#include "src/game_config/condition_config.h"
#include "src/random/random.h"
#include "src/return_code/error_code.h"

namespace common
{
    MissionsComp::MissionsComp()
        : config_(&MissionConfig::GetSingleton())
    {
        for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_MAX; ++i)
        {
            classify_missions_.emplace(i, UInt32Set{});
        }
        if (config_->HasMainSubTypeCheck())
        {
            reg().emplace<CheckSubType>(entity());
        }
    }

    MissionsComp::MissionsComp(IMissionConfig* config)
        : MissionsComp()          
    {
        config_ = config;
    }

    uint32_t MissionsComp::GetReward(uint32_t missin_id)
    {
        auto rmid = complete_ids_.mutable_can_reward_mission_id();
        auto it = complete_ids_.mutable_can_reward_mission_id()->find(missin_id);
        if (it == rmid->end())
        {
            return RET_MISSION_GET_REWARD_NO_MISSION_ID;
        }
        rmid->erase(missin_id);
        return RET_OK;
    }

    uint32_t MissionsComp::Accept(const MakeMissionP& param)
    {
        auto mission_id = param.missionid_;
        if (missions_.missions().count(mission_id))
        {
            return RET_MISSION_ID_REPTEATED;
        }
        if (complete_ids_.missions().count(mission_id))
        {
            return RET_MISSION_COMPLETE;
        }
        auto conditions = param.conditions_id_;
        if (nullptr == conditions)
        {
            return RET_MISSION_NO_CONDITION;
        }
        if (!config_->HasKey(mission_id))
        {
            return RET_TABLE_ID_ERROR;
        }
        auto mission_sub_type = config_->mission_sub_type(mission_id);
        auto mission_type = config_->mission_type(mission_id);
        bool check_sub_mission = config_->HasMainSubTypeCheck() &&
            mission_sub_type > 0 &&
            reg().any_of<CheckSubType>(entity());
        if (check_sub_mission)
        {
            UInt32PairSet::value_type p(mission_type, mission_sub_type);
            auto it = type_set_.find(p);
            CheckCondtion(it != type_set_.end(), RET_MISSION_TYPE_REPTEATED);
        }
        Mission m;
        m.set_id(mission_id);
        for (int32_t i = 0; i < conditions->size(); ++i)
        {
            auto condition_id = conditions->Get(i);
            auto p = condition_config::GetSingleton().get(condition_id);
            if (nullptr == p)
            {
                continue;
            }
            auto pcs = m.add_conditions();
            pcs->set_id(condition_id);
            classify_missions_[p->condition_type()].emplace(mission_id);
        }
        missions_.mutable_missions()->insert({ mission_id, std::move(m) });
        if (check_sub_mission)
        {
            UInt32PairSet::value_type p(mission_type, mission_sub_type);
            type_set_.emplace(p);
        }
        return RET_OK;
    }

    uint32_t MissionsComp::AcceptCheck(const MakeMissionP& param)
    {
        return RET_OK;
    }

    uint32_t MissionsComp::Abandon(uint32_t mission_id)
    {
        missions_.mutable_missions()->erase(mission_id);
        complete_ids_.mutable_missions()->erase(mission_id);
        complete_ids_.mutable_can_reward_mission_id()->erase(mission_id);
        auto begin_times = reg().try_get<MissionBeginTime>(entity());
        if (nullptr != begin_times)
        {
            begin_times->mutable_mission_begin_time()->erase(mission_id);
        }
        DelClassify(mission_id);
        return RET_OK;
    }

    void MissionsComp::TriggerConditionEvent(const ConditionEvent& c)
    {
        if (c.condtion_ids_.empty())
        {
            return;
        }
        auto mm = missions_.mutable_missions();
        auto it = classify_missions_.find(c.condition_type_);
        if (it == classify_missions_.end())
        {
            return;
        }
        TempCompleteList temp_complete;
        for (auto lmit : it->second)
        {
            auto mit = mm->find(lmit);
            if (mit == mm->end())
            {
                continue;
            }
            auto& mission = mit->second;
            if (!TriggerCondition(c, mission))
            {
                continue;
            }
            bool all_complete = true;
            for (int32_t i = 0; i < mission.conditions_size(); ++i)
            {
                if (mission.mutable_conditions(i)->status() == E_CONDITION_COMPLETE)
                {
                    continue;
                }
                all_complete = false;
                break;
            }
            if (!all_complete)
            {
                break;
            }
            mission.set_status(E_MISSION_COMPLETE);
            mission.clear_conditions();
            temp_complete.emplace(mission.id());
            mm->erase(mit);
            // can not use mission and mit 
        }

        OnCompleteMission(c, temp_complete);
    }

    void MissionsComp::CompleteAllMission()
    {
        for (auto& meit : missions_.missions())
        {
            complete_ids_.mutable_missions()->insert({ meit.first, false });
        }
        missions_.mutable_missions()->clear();
    }

    void MissionsComp::DelClassify(uint32_t mission_id)
    {
        auto& cs = config_->condition_id(mission_id);
        for (int32_t i = 0; i < cs.size(); ++i)
        {
            auto cp = condition_config::GetSingleton().get(cs.Get(i));
            if (nullptr == cp)
            {
                continue;
            }
            classify_missions_[cp->condition_type()].erase(mission_id);
        }
        auto mission_sub_type = config_->mission_sub_type(mission_id);
        auto mission_type = config_->mission_type(mission_id);
        TypeSubTypeSet::value_type p(mission_type, mission_sub_type);
        type_set_.erase(p);
    }

    bool MissionsComp::TriggerCondition(const ConditionEvent& c, Mission& mission)
    {
        if (c.condtion_ids_.empty())
        {
            return false;
        }
        auto& row_condtion1 = c.condtion_ids_[E_CONDITION_1];
        //compare condition
        bool condition_change = false;
        for (int32_t i = 0; i < mission.conditions_size(); ++i)
        {
            auto condition = mission.mutable_conditions(i);
            if (condition->status() == E_CONDITION_COMPLETE)
            {
                continue;
            }
            auto p = condition_config::GetSingleton().get(condition->id());
            if (nullptr == p)
            {
                continue;
            }
            if (c.condition_type_ != p->condition_type())
            {
                continue;
            }
            bool conform = false;
            for (int32_t ci = 0; ci < p->condition1_size(); ++ci)
            {
                if (row_condtion1 != p->condition1(ci))
                {
                    continue;
                }
                conform = true;
                break;
            }
            if (!conform)
            {
                continue;
            }
            condition_change = true;
            condition->set_progress(c.ammount_ + condition->progress());
            // to client
            if (condition->progress() < p->amount())
            {
                continue;
            }
            condition->set_progress(p->amount());
            condition->set_status(E_CONDITION_COMPLETE);
            // to client
        }
        return condition_change;
    }

    void MissionsComp::OnCompleteMission(const ConditionEvent& c, const TempCompleteList& temp_complete)
    {
        if (temp_complete.empty())
        {
            return;
        }
        bool reward = reg().any_of<MissionReward>(entity());
        for (auto& mission_id : temp_complete)
        {
            complete_ids_.mutable_missions()->insert({ mission_id, true });
            if (reward && config_->reward_id(mission_id) > 0)
            {
                complete_ids_.mutable_can_reward_mission_id()->insert({ mission_id, false });
            }

            DelClassify(mission_id);
            auto& next_missions = config_->next_mission_id(mission_id);
            auto next_time_accpet = reg().try_get<NextTimeAcceptMission>(entity());
            if (nullptr == next_time_accpet)
            {
                for (int32_t i = 0; i < next_missions.size(); ++i)
                {
                    auto next_condition_id = next_missions.Get(i);
                    MakeMissionP param{ next_condition_id,
                        config_->condition_id(next_condition_id) };
                    Accept(param);
                }
            }
            else
            {
                for (int32_t i = 0; i < next_missions.size(); ++i)
                {
                    next_time_accpet->next_time_accept_mission_id_.emplace(next_missions.Get(i));
                }
            }
        }

        ConditionEvent ce{ E_CONDITION_COMPLELTE_MISSION, {}, 1 };
        for (auto& it : temp_complete)
        {
            ce.condtion_ids_ = { it };
            TriggerConditionEvent(ce);
        }
    }

    uint32_t RandomMision(const MakePlayerMissionP& param, MissionsComp& ms)
    {
        auto mission_id = param.mission_id_;
        auto mrow = mission_config::GetSingleton().get(mission_id);
        if (nullptr == mrow)
        {
            return RET_TABLE_ID_ERROR;
        }
        MakeMissionP mp{ mission_id, mrow->condition_id()};
        if (mrow->random_condition_pool_size() > 0)
        {
            MakeMissionP::PBUint32V v;
            auto i = Random::GetSingleton().Rand<int32_t>(0, mrow->random_condition_pool_size() - 1);
            *v.Add() = mrow->random_condition_pool().Get(i);
            mp.conditions_id_ = &v;
            RET_CHECK_RET(ms.Accept(mp));
        }
        else
        {
            RET_CHECK_RET(ms.Accept(mp));
        }
        return RET_OK;
    }
}//namespace common