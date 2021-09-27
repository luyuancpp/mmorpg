#include "missions.h"
#include "condition.h"

#include "src/game_config/condition_config.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/factories/mission_factories.h"
#include "src/return_code/error_code.h"

namespace common
{
    Missions::Missions()
    {
        for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_MAX; ++i)
        {
            type_missions_.emplace(i, UI32USet{});
        }
    }

    uint32_t Missions::MakeMission(const MakeMissionParam& param)
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
        return RET_OK;
    }

    void Missions::CompleteAllMission()
    {
        for (auto& meit : missions_.missions())
        {
            complete_ids_.mutable_missions()->insert({ meit.first, false });
        }
        missions_.mutable_missions()->clear();
    }

}//namespace common