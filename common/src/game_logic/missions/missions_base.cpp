#include "missions_base.h"


#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/game_config/condition_config.h"
#include "src/random/random.h"
#include "src/return_code/error_code.h"

namespace common
{

    MissionsComp::MissionsComp()
        : MissionsComp(&MissionConfig::GetSingleton()){}

    MissionsComp::MissionsComp(IMissionConfig* config)
        : config_(config)
    {
        for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_MAX; ++i)
        {
             event_missions_classify_.emplace(i, UInt32Set{});
        }
        if (config_->HasMainSubTypeCheck())
        {
            reg.emplace<CheckSubType>(entity());
        }
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

    uint32_t MissionsComp::Accept(const AcceptMissionBaseP& param)
    {
        auto mission_id = param.mission_id_;
        if (missions_.missions().count(mission_id) > 0)
        {
            return RET_MISSION_ID_REPTEATED;
        }
        if (complete_ids_.missions().count(mission_id) > 0)
        {
            return RET_MISSION_COMPLETE;
        }
        auto condition_id_ = param.conditions_id_;
        if (nullptr == condition_id_)
        {
            return RET_MISSION_NO_CONDITION;
        }
        if (!config_->HasKey(mission_id))
        {
            return RET_TABLE_ID_ERROR;
        }
        auto mission_sub_type = config_->mission_sub_type(mission_id);
        auto mission_type = config_->mission_type(mission_id);
        bool check_type_filter = config_->HasMainSubTypeCheck() &&  mission_sub_type > 0 && reg.any_of<CheckSubType>(entity());
        if (check_type_filter)
        {
            UInt32PairSet::value_type p(mission_type, mission_sub_type);
            CheckCondtion(type_filter_.find(p) != type_filter_.end(), RET_MISSION_TYPE_REPTEATED);
        }
        Mission m;
        m.set_id(mission_id);
        for (int32_t i = 0; i < condition_id_->size(); ++i)
        {
            auto condition_id = condition_id_->Get(i);
            auto p = condition_config::GetSingleton().get(condition_id);
            if (nullptr == p)
            {
                LOG_ERROR << "has not condtion" << condition_id;
                continue;
            }
            auto pcs = m.add_conditions();
            pcs->set_id(condition_id);
             event_missions_classify_[p->condition_type()].emplace(mission_id);
        }
        missions_.mutable_missions()->insert({ mission_id, std::move(m) });
        if (check_type_filter)
        {
            UInt32PairSet::value_type p(mission_type, mission_sub_type);
            type_filter_.emplace(p);
        }
        return RET_OK;
    }

    uint32_t MissionsComp::AcceptCheck(const AcceptMissionBaseP& param)
    {
        return RET_OK;
    }

    uint32_t MissionsComp::Abandon(uint32_t mission_id)
    {
        missions_.mutable_missions()->erase(mission_id);
        complete_ids_.mutable_missions()->erase(mission_id);
        complete_ids_.mutable_can_reward_mission_id()->erase(mission_id);
        auto begin_times = reg.try_get<MissionBeginTime>(entity());
        if (nullptr != begin_times)
        {
            begin_times->mutable_mission_begin_time()->erase(mission_id);
        }
        DelClassify(mission_id);
        return RET_OK;
    }

    void MissionsComp::CompleteAllMission()
    {
        for (auto& meit : missions_.missions())
        {
            complete_ids_.mutable_missions()->insert({ meit.first, false });
        }
        missions_.mutable_missions()->clear();
    }

    void MissionsComp::receive(const ConditionEvent& c)
    {
        if (c.match_condtion_ids_.empty())
        {
            return;
        }
        auto mm = missions_.mutable_missions();
        auto it =  event_missions_classify_.find(c.type_);
        if (it ==  event_missions_classify_.end())
        {
            return;
        }
        TempCompleteList temp_complete;
        auto& mission_list = it->second;
        for (auto lit : mission_list)
        {
            auto mit = mm->find(lit);
            if (mit == mm->end())
            {
                continue;
            }
            auto& mission = mit->second;
            if (!TriggerChangeByMatchCondition(c, mission))
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

        OnMissionComplete(c, temp_complete);
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
             event_missions_classify_[cp->condition_type()].erase(mission_id);
        }
        TypeSubTypeSet::value_type p(config_->mission_type(mission_id), config_->mission_sub_type(mission_id));
        type_filter_.erase(p);
    }

    bool MissionsComp::TriggerChangeByMatchCondition(const ConditionEvent& c, Mission& mission)
    {
        if (c.match_condtion_ids_.empty())
        {
            return false;
         }
        auto& row_condtion1 = c.match_condtion_ids_[E_CONDITION_1];//to do condition 2
        //compare condition
        bool mission_change = false;
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
            if (c.type_ != p->condition_type())
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
            mission_change = true;
            condition->set_progress(c.ammount_ + condition->progress());

            static std::vector<std::function<bool(int32_t, int32_t)>> f_c{
                {[](int32_t a, int32_t b) {return a >=   b; }},
                {[](int32_t a, int32_t b) {return a > b; }},
                {[](int32_t a, int32_t b) {return a <= b; }},
                {[](int32_t a, int32_t b) {return a < b; }},
                {[](int32_t a, int32_t b) {return a == b; }},
            };

            std::size_t operator_id = std::size_t(p->operation());

            if (!(operator_id >= 0 && operator_id < f_c.size()))
            {
                operator_id = 0;
            }

            if (!f_c[operator_id](condition->progress(), p->amount()))
            {
                continue;
            }

            // to client
            condition->set_progress(p->amount());
            condition->set_status(E_CONDITION_COMPLETE);
            // to client
        }
        return mission_change;
    }

    void MissionsComp::OnMissionComplete(const ConditionEvent& c, const TempCompleteList& temp_complete)
    {
        if (temp_complete.empty())
        {
            return;
        }
        bool reward = reg.any_of<MissionReward>(entity());
        for (auto& mission_id : temp_complete)
        {
            complete_ids_.mutable_missions()->insert({ mission_id, true });
            if (reward && config_->reward_id(mission_id) > 0)
            {
                complete_ids_.mutable_can_reward_mission_id()->insert({ mission_id, false });
            }
            DelClassify(mission_id);
            auto& next_missions = config_->next_mission_id(mission_id);
            auto next_time_accpet = reg.try_get<NextTimeAcceptMission>(entity());
            if (nullptr == next_time_accpet)
            {
                for (int32_t i = 0; i < next_missions.size(); ++i)
                {
                    auto next_condition_id = next_missions.Get(i);
                    AcceptMissionBaseP param{ next_condition_id,
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
            ce.match_condtion_ids_ = { it };
            receive(ce);
        }
    }

}//namespace common