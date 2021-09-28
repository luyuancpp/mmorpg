#include "missions.h"

#include "src/game_logic/game_registry.h"
#include "src/return_code/error_code.h"

namespace common
{
    uint32_t RandomMision(const MakePlayerMissionParam& param, Missions<mission_config, mission_row>& ms)
    {
        auto mission_id = param.mission_id_;
        auto mrow = mission_config::GetSingleton().key_id(mission_id);
        if (nullptr == mrow)
        {
            return RET_TABLE_ID_ERROR;
        }
        MakeMissionParam mp{ param.e_, mission_id, mrow->condition_id()};
        if (mrow->random_condition_pool_size() > 0)
        {
            MakeMissionParam::ConditionV v;
            auto i = Random::GetSingleton().Rand<int32_t>(0, mrow->random_condition_pool_size() - 1);
            *v.Add() = mrow->random_condition_pool().Get(i);
            mp.condition_id_ = &v;
            RET_CHECK_RET(ms.MakeMission(mp));
        }
        else
        {
            RET_CHECK_RET(ms.MakeMission(mp));
        }
        return RET_OK;
    }
}//namespace common