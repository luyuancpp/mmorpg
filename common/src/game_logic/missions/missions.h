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

        void CompleteAllMission()
        {
            for (auto& meit : missions_.missions())
            {
                complete_ids_.mutable_missions()->insert({ meit.first, false });
            }
            missions_.mutable_missions()->clear();
        }
    private:
        MissionMap missions_;
        CompleteMissionsId complete_ids_;  
        TypeMissionIdMap type_missions_;
        UI32PairSet type_set_;
    };


    uint32_t RandomMision(const MakePlayerMissionParam& param, Missions<mission_config, mission_row>& ms);
}//namespace common

#endif // !COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_
