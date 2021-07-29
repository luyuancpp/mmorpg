#include "factories.h"

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/common_type/common_type.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/random/random.h"
#include "src/return_code/return_notice_code.h"

#include "comp.pb.h"

namespace common
{

entt::entity MakeMissionMap()
{
    auto e = reg().create();
    reg().emplace<MissionMap>(e);
    reg().emplace<TypeMissionEntityMap>(e);
    return e;
}

entt::entity MakePlayerMissionMap()
{
    auto e = MakeMissionMap();
    reg().emplace<CompleteMissionsId>(e);
    reg().emplace<UI32PairSet>(e);
    return e;
}

uint32_t MakeMission(const MakeMissionParam& param)
{
    auto missions = reg().get<MissionMap>(param.e_).mutable_missions();
    if (missions->count(param.mision_id_))
    {
        return RET_MISSION_ID_REPTEATED;
    }
    if (nullptr == param.condition_id_)
    {
        return RET_MISSION_NO_CONDITION;
    }
    Mission m;
    m.set_id(param.mision_id_);
    for (int32_t i = 0; i < param.condition_id_->size(); ++i)
    {
        auto pcs = m.add_conditions();
        pcs->set_id(param.condition_id_->Get(i));
    }
    missions->insert({ param.mision_id_, std::move(m) });
    return RET_OK;
}

uint32_t MakePlayerMission(const MakePlayerMissionParam& param)
{
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(param.mision_id_);
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
    MakeMissionParam mp{ param.e_, param.mision_id_, cids ->condition_id(), param.op_};
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


