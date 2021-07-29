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
    I32V condtion_ids_{};
    uint32_t ammount_{ 1 };
};

void CompleteAllMission(entt::entity, uint32_t op);
void TriggerConditionEvent(const ConditionEvent&);

}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_SYS_MISSION_SYS_H_
