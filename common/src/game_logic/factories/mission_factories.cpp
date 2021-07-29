#include "mission_factories.h"

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/common_type/common_type.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/random/random.h"
#include "src/return_code/return_notice_code.h"

#include "comp.pb.h"

namespace common
{

bool CheckMissonAutoReward(uint32_t mission_id)
{
    auto p = MissionJson::GetSingleton().Primary1KeyRow(mission_id);
    return nullptr != p && p->auto_reward() > 0;
}

entt::entity MakeMissionMap()
{
    auto e = reg().create();
    reg().emplace<MissionMap>(e);
    reg().emplace<CompleteMissionsId>(e);
    auto type_missions =  reg().emplace<TypeMissionIdMap>(e);
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_MAX; ++i)
    {
        type_missions.emplace(i, UI32USet{});
    }
    return e;
}

entt::entity MakePlayerMissionMap()
{
    auto e = MakeMissionMap();
    reg().emplace<UI32PairSet>(e);
    //reg().emplace<MissionAutoRewardCallback>(e, MissionAutoRewardCallback(CheckMissonAutoReward));
    return e;
}

uint32_t MakeMission(const MakeMissionParam& param)
{
    auto missions = reg().get<MissionMap>(param.e_).mutable_missions();
    auto mission_id = param.mission_id_;
    if (missions->count(mission_id))
    {
        return RET_MISSION_ID_REPTEATED;
    }
    if (nullptr == param.condition_id_)
    {
        return RET_MISSION_NO_CONDITION;
    }
    Mission m;
    m.set_id(mission_id);
    auto& type_missions = reg().get<TypeMissionIdMap>(param.e_);
    for (int32_t i = 0; i < param.condition_id_->size(); ++i)
    {
        auto pcs = m.add_conditions();
        pcs->set_id(param.condition_id_->Get(i));
        auto p = ConditionJson::GetSingleton().PrimaryKeyRow(pcs->id());
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
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(param.mission_id_);
    if (nullptr == cids)
    {
        return RET_TABLE_ID_ERROR;
    }
   
    if ( cids->mission_sub_type() > 0)
    {
        UI32PairSet::value_type p(cids->mission_type(), cids->mission_sub_type());
        auto& type_set = reg().get<UI32PairSet>(param.e_);
        auto it = type_set.find(p);
        if (it != type_set.end())
        {
            return RET_MISSION_TYPE_REPTEATED;
        }
    }    
    MakeMissionParam mp{ param.e_, mission_id, cids ->condition_id(), param.op_};
    if (cids->random_condition_pool_size() > 0)
    {
        MakeMissionParam::ConditionV v;
        auto i = Random::GetSingleton().Rand<int32_t>(0, cids->random_condition_pool_size() - 1);
        *v.Add() = cids->random_condition_pool().Get(i);
        mp.condition_id_ = &v;
        RET_CHECK_RET(MakeMission(mp));
    }
    else
    {
        RET_CHECK_RET(MakeMission(mp));
    }
    if (cids->mission_sub_type() > 0)
    {
        UI32PairSet::value_type p(cids->mission_type(), cids->mission_sub_type());
        auto& type_set = reg().get<UI32PairSet>(param.e_);
        type_set.emplace(p);
    }
    return RET_OK;
}

}//namespace common


