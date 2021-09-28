#include "mission_factories.h"

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_config/condition_config.h"
#include "src/game_config/mission_config.h"
#include "src/common_type/common_type.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/random/random.h"
#include "src/return_code/error_code.h"

#include "comp.pb.h"

namespace common
{

void RemoveMissionTypeSubType(entt::entity e, uint32_t mission_type, uint32_t mission_sub_type);

bool CheckPlayerMissonAutoReward(uint32_t mission_id)
{
    auto p = mission_config::GetSingleton().key_id(mission_id);
    return nullptr != p && p->auto_reward() > 0;
}

void OnPlayerCompleteMission(entt::entity e, uint32_t mission_id, CompleteMissionsId& cm)
{
    cm.mutable_missions()->insert({ mission_id, true});
    auto mrow = mission_config::GetSingleton().key_id(mission_id);
    if (nullptr == mrow)
    {
        return;
    }
    if (mrow->reward_id() > 0)
    {
        cm.mutable_can_reward_mission_id()->insert({ mission_id, false });
    }
}

entt::entity MakeMissionMap()
{
    auto e = reg().create();
    reg().emplace<MissionMap>(e);
    reg().emplace<CompleteMissionsId>(e);

    auto& type_missions =  reg().emplace<TypeMissionIdMap>(e);
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_MAX; ++i)
    {
        type_missions.emplace(i, UI32USet{});
    }
    return e;
}

entt::entity MakePlayerMissionMap()
{
    auto e = MakeMissionMap();
    reg().emplace<TypeSubTypeSet>(e);
    //reg().emplace<MissionAutoRewardCallback>(e, MissionAutoRewardCallback(CheckMissonAutoReward));
    reg().emplace<CompleteMissionCallback>(e, OnPlayerCompleteMission);
    return e;
}

uint32_t MakeMission(const MakeMissionParam& param)
{
    auto e = param.e_;
    auto missions = reg().get<MissionMap>(e).mutable_missions();
    auto mission_id = param.mission_id_;
    if (missions->count(mission_id))
    {
        return RET_MISSION_ID_REPTEATED;
    }
    auto& cms = reg().get<CompleteMissionsId>(e);
    if (cms.missions().count(mission_id))
    {
        return RET_MISSION_COMPLETE;
    }
    auto condition_id = param.condition_id_;
    if (nullptr == condition_id)
    {
        return RET_MISSION_NO_CONDITION;
    }    
    Mission m;
    m.set_id(mission_id);
    auto& type_missions = reg().get<TypeMissionIdMap>(e);
    for (int32_t i = 0; i < condition_id->size(); ++i)
    {
        auto pcs = m.add_conditions();
        pcs->set_id(condition_id->Get(i));
        auto p = condition_config::GetSingleton().key_id(pcs->id());
        if (nullptr == p)
        {
            continue;
        }
        type_missions[p->condition_type()].emplace(mission_id);
    }
    missions->insert({ mission_id, std::move(m) });

    return RET_OK;
}

uint32_t MakePlayerMission(const MakePlayerMissionParam& param)
{
    auto mission_id = param.mission_id_;
    auto mrow = mission_config::GetSingleton().key_id(param.mission_id_);
    if (nullptr == mrow)
    {
        return RET_TABLE_ID_ERROR;
    }   
    MakeMissionParam mp{ param.e_, mission_id, mrow->condition_id(), param.op_};
    if (mrow->random_condition_pool_size() > 0)
    {
        MakeMissionParam::ConditionV v;
        auto i = Random::GetSingleton().Rand<int32_t>(0, mrow->random_condition_pool_size() - 1);
        *v.Add() = mrow->random_condition_pool().Get(i);
        mp.condition_id_ = &v;
        RET_CHECK_RET(MakeMission(mp));
    }
    else
    {
        RET_CHECK_RET(MakeMission(mp));
    }

    return RET_OK;
}

}//namespace common


