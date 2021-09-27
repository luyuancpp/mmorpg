#ifndef COMMON_SRC_GAME_LOGIC_SYS_MISSION_SYS_H_
#define COMMON_SRC_GAME_LOGIC_SYS_MISSION_SYS_H_

#include "entt/src/entt/entity/fwd.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/op_code.h"

namespace common
{

struct ConditionEvent
{
 
    bool CheckType() const;

    entt::entity e_{};
    uint32_t condition_type_{0};
    UI32V condtion_ids_{};
    uint32_t ammount_{ 1 };
    uint32_t op_{ 0 };
};

struct MissionIdParam
{
    entt::entity e_{};
    uint32_t missin_id_{ 0 };
    uint32_t op_{ 0 };
};

void TriggerConditionEvent(const ConditionEvent&);
uint32_t GiveMission(const MissionIdParam&);
uint32_t GetMissionReward(const MissionIdParam&);
void RemoveMission(const MissionIdParam&);
bool IsAcceptedMission(const MissionIdParam&);
bool IsCompleteMission(const MissionIdParam&);
void RemoveMissionTypeSubType(entt::entity e, uint32_t mission_type, uint32_t mission_sub_type);
}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_SYS_MISSION_SYS_H_
