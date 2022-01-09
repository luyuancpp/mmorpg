#include "player_mission.h"

#include "src/random/random.h"
#include "src/return_code/error_code.h"

namespace common
{
AcceptMissionP::AcceptMissionP(uint32_t mission_id)
: AcceptMissionBaseP(mission_id)
{
auto mrow = mission_config::GetSingleton().get(mission_id);
if (nullptr == mrow)
{
    return;
}
conditions_id_ = &mrow->condition_id();
}


uint32_t RandomMision(const AcceptPlayerRandomMissionP& param, MissionsComp& ms)
{
    auto mission_id = param.mission_id_;
    auto mrow = mission_config::GetSingleton().get(mission_id);
    if (nullptr == mrow)
    {
        return RET_TABLE_ID_ERROR;
    }
    AcceptMissionBaseP mp{ mission_id, mrow->condition_id() };
    if (mrow->random_condition_pool_size() > 0)
    {
        AcceptMissionBaseP::PBUint32V v;
        auto i = Random::GetSingleton().Rand<int32_t>(0, mrow->random_condition_pool_size() - 1);
        *v.Add() = mrow->random_condition_pool().Get(i);
        mp.conditions_id_ = &v;
        mp.is_random_condition_ = true;
        RET_CHECK_RET(ms.Accept(mp));
    }
    else
    {
        RET_CHECK_RET(ms.Accept(mp));
    }
    return RET_OK;
}


}//namespace common