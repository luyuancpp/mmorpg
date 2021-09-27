#ifndef COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_
#define COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_

#include "entt/src/entt/entity/entity.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/entity_class/entity_class.h"

#include "comp.pb.h"

namespace common
{
    struct MakeMissionParam;

    class Missions : public EntityClass
    {
    public:
        using TypeMissionIdMap = std::unordered_map<uint32_t, UI32USet>;
        Missions();

        std::size_t mission_size()const { return missions_.missions().size(); }
        std::size_t completemission_size()const { return complete_ids_.missions().size(); }

        uint32_t MakeMission(const MakeMissionParam& param);
        void CompleteAllMission();
    private:
        MissionMap missions_;
        CompleteMissionsId complete_ids_;  
        TypeMissionIdMap type_missions_;
    };
}//namespace common

#endif // !COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_
