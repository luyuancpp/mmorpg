#include "mission_sys.hpp"

#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/factories/mission_factories.h"
#include "src/return_code/return_notice_code.h"

#include "comp.pb.h"

namespace common
{

bool ConditionEvent::CheckType() const
{
    return condition_type_ < E_CONDITION_MAX;
}

void CompleteAllMission(entt::entity e, uint32_t op)
{
    auto& mm = reg().get<MissionMap>(e);
    auto& cm = reg().get<CompleteMissionsId>(e);
    for (auto& meit : mm.missions())
    {
        cm.mutable_missions()->insert({ meit.first, false});
    }
    reg().remove<MissionMap>(e);
}

bool TriggerCondition(const ConditionEvent& c, Mission& mission)
{
    auto& row_condtion1 = c.condtion_ids_[E_CONDITION_1];
    //compera condition
    bool condition_change = false;
    for (int32_t i = 0; i < mission.conditions_size(); ++i)
    {
        auto condition = mission.mutable_conditions(i);
        if (condition->status() == E_CONDITION_COMPLETE)
        {
            continue;
        }
        auto p = ConditionJson::GetSingleton().PrimaryKeyRow(condition->id());
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

void AcceptNextMission(const ConditionEvent& c)
{
    auto e = c.e_;
    auto& type_missions = reg().get<TypeMissionIdMap>(e);
    auto& temp_complete = reg().get<TempCompleteList>(e);
    for (auto& it : temp_complete)
    {
        auto p = MissionJson::GetSingleton().PrimaryKeyRow(it);
        if (nullptr == p)
        {
            continue;
        }
        for (int32_t i = 0; i < p->condition_id_size(); ++i)
        {
            auto cp = ConditionJson::GetSingleton().PrimaryKeyRow(p->condition_id(i));
            if (nullptr == cp)
            {
                continue;
            }
            type_missions[cp->condition_type()].erase(it);
        }

        auto next_time_accpet = reg().try_get<NextTimeAcceptMission>(e);
        if (nullptr == next_time_accpet)
        {
            for (int32_t i = 0; i < p->next_mission_id_size(); ++i)
            {
                MakePlayerMissionParam param{ e,   p->next_mission_id(i),  c.op_ };
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
}

void TriggerConditionEvent(const ConditionEvent& c)
{
    if (c.condtion_ids_.empty())
    {
        return;
    }

    auto e = c.e_;
    auto mm = reg().get<MissionMap>(c.e_).mutable_missions();
    auto type_missions = reg().get<TypeMissionIdMap>(e);
    auto& cm = reg().get<CompleteMissionsId>(e);
    auto complete_callback = reg().try_get<CompleteMissionCallback>(e);
    auto it = type_missions.find(c.condition_type_);// aready check
    if (it == type_missions.end())
    {
        return;
    }
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
        reg().get<TempCompleteList>(e).emplace(mission.id());
        if (nullptr != complete_callback)
        {
            complete_callback->operator()(e, mit->first, cm);
        }
        else
        {
            cm.mutable_missions()->insert({ mission.id(), false });
        }
        mm->erase(mit);
        // can not use mission and mit 
    }

    AcceptNextMission(c);
}

uint32_t GiveMission(const MissionIdParam& gum)
{
    return RET_OK;
}

bool IsAcceptedMission(const MissionIdParam& icm)
{
    auto& cmi = reg().get<MissionMap>(icm.e_);
    return cmi.missions().find(icm.missin_id_) != cmi.missions().end();
}

bool IsCompleteMission(const MissionIdParam& icm)
{
    auto& cmi = reg().get<CompleteMissionsId>(icm.e_);
    return cmi.missions().find(icm.missin_id_) != cmi.missions().end();
}

}//namespace common

