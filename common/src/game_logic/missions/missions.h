#ifndef COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_
#define COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_

#include "entt/src/entt/entity/entity.hpp"

#include "condition.h"
#include "src/common_type/common_type.h"
#include "src/game_config/condition_config.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/game_logic/entity_class/entity_class.h"
#include "src/game_logic/missions/missions_config_template.h"
#include "src/game_logic/factories/mission_factories.h"
#include "src/game_logic/sys/mission_sys.hpp"
#include "src/random/random.h"
#include "src/return_code/error_code.h"

#include "comp.pb.h"

namespace common
{
    template<typename Config, typename ConfigRow>
    class Missions : public EntityClass
    {
    public:
        using TypeMissionIdMap = std::unordered_map<uint32_t, UI32USet>;
        using MissionConfig = MissionConfig<Config, ConfigRow>;
        Missions()
        {
            for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_MAX; ++i)
            {
                type_missions_.emplace(i, UI32USet{});
            }
            if (MissionConfig::GetSingleton().HasMainSubTypeCheck())
            {
                reg().emplace<CheckSubType>(entity());
            }            
        }

        const MissionMap& missions() { return missions_; }
        std::size_t mission_size()const { return missions_.missions().size(); }
        std::size_t completemission_size()const { return complete_ids_.missions().size(); }
        std::size_t type_set_size()const { return type_set_.size(); }

        uint32_t MakeMission(const MakeMissionParam& param)
        {
            auto mission_id = param.mission_id_;
            if (missions_.missions().count(mission_id))
            {
                return RET_MISSION_ID_REPTEATED;
            }
            if (complete_ids_.missions().count(mission_id))
            {
                return RET_MISSION_COMPLETE;
            }
            auto condition_id = param.condition_id_;
            if (nullptr == condition_id)
            {
                return RET_MISSION_NO_CONDITION;
            }
            auto mrow = MissionConfig::GetSingleton().key_id(mission_id);
            if (nullptr == mrow)
            {
                return RET_TABLE_ID_ERROR;
            }
            auto mission_sub_type = MissionConfig::GetSingleton().mission_sub_type(mission_id);
            auto mission_type = MissionConfig::GetSingleton().mission_type(mission_id);
            bool check_sub_mission = MissionConfig::GetSingleton().HasMainSubTypeCheck() &&
            mission_sub_type > 0 &&
            reg().any_of<CheckSubType>(entity());
            if (check_sub_mission)
            {
                UI32PairSet::value_type p(mission_type, mission_sub_type);
                auto it = type_set_.find(p);
                RetrunIfError(it != type_set_.end(), RET_MISSION_TYPE_REPTEATED);
            }
            Mission m;
            m.set_id(mission_id);
            for (int32_t i = 0; i < condition_id->size(); ++i)
            {
                auto pcs = m.add_conditions();
                pcs->set_id(condition_id->Get(i));
                auto p = condition_config::GetSingleton().key_id(pcs->id());
                if (nullptr == p)
                {
                    continue;
                }
                type_missions_[p->condition_type()].emplace(mission_id);
            }
            missions_.mutable_missions()->insert({ mission_id, std::move(m) });
            if (check_sub_mission)
            {
                UI32PairSet::value_type p(mission_type, mission_sub_type);
                type_set_.emplace(p);
            }
            return RET_OK;
        }

        void OnCompleteMission(const ConditionEvent& c, const TempCompleteList& temp_complete)
        {
            if (temp_complete.empty())
            {
                return;
            }
            for (auto& it : temp_complete)
            {
                auto p = mission_config::GetSingleton().key_id(it);
                RemoveMissionTypeSubType(it);
                if (nullptr == p)
                {
                    continue;
                }
                for (int32_t i = 0; i < p->condition_id_size(); ++i)
                {
                    auto cp = condition_config::GetSingleton().key_id(p->condition_id(i));
                    if (nullptr == cp)
                    {
                        continue;
                    }
                    type_missions_[cp->condition_type()].erase(it);
                }

                auto next_time_accpet = reg().try_get<NextTimeAcceptMission>(entity());
                if (nullptr == next_time_accpet)
                {
                    for (int32_t i = 0; i < p->next_mission_id_size(); ++i)
                    {
                        MakePlayerMissionParam param{ entity(),   p->next_mission_id(i),  c.op_ };
                        MakePlayerMission(param);
                    }
                }
                else
                {
                    for (int32_t i = 0; i < p->next_mission_id_size(); ++i)
                    {
                        next_time_accpet->next_time_accept_mission_id_.emplace(p->next_mission_id(i));
                    }
                }
            }

            ConditionEvent ce{ entity(), E_CONDITION_COMPLELTE_MISSION, {}, 1 };
            for (auto& it : temp_complete)
            {
                ce.condtion_ids_ = { it };
                TriggerConditionEvent(ce);
            }
        }

        bool TriggerCondition(const ConditionEvent& c, Mission& mission)
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
                auto p = condition_config::GetSingleton().key_id(condition->id());
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

        void TriggerConditionEvent(const ConditionEvent& c)
        {
            if (c.condtion_ids_.empty())
            {
                return;
            }
            auto mm = missions_.mutable_missions();
            auto complete_callback = reg().try_get<CompleteMissionCallback>(entity());
            auto it = type_missions_.find(c.condition_type_);
            if (it == type_missions_.end())
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
                if (nullptr != complete_callback)
                {
                    complete_callback->operator()(entity(), mit->first, complete_ids_);
                }
                else
                {
                    complete_ids_.mutable_missions()->insert({ mission.id(), false });
                }
                mm->erase(mit);
                // can not use mission and mit 
            }

            OnCompleteMission(c, temp_complete);
        }

        void CompleteAllMission()
        {
            for (auto& meit : missions_.missions())
            {
                complete_ids_.mutable_missions()->insert({ meit.first, false });
            }
            missions_.mutable_missions()->clear();
        }
    private:
        void RemoveMissionTypeSubType(uint32_t mission_id)
        {
            auto mrow = MissionConfig::GetSingleton().key_id(mission_id);
            if (nullptr == mrow)
            {
                return;
            }
            auto mission_sub_type = MissionConfig::GetSingleton().mission_sub_type(mission_id);
            auto mission_type = MissionConfig::GetSingleton().mission_type(mission_id);
            TypeSubTypeSet::value_type p(mission_type, mission_sub_type);
            type_set_.erase(p);
        }

        MissionMap missions_;
        CompleteMissionsId complete_ids_;  
        TypeMissionIdMap type_missions_;
        UI32PairSet type_set_;
    };


    uint32_t RandomMision(const MakePlayerMissionParam& param, Missions<mission_config, mission_row>& ms);
}//namespace common

#endif // !COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_
