#ifndef COMMON_SRC_GAME_LOIGC_COMP_H_
#define COMMON_SRC_GAME_LOIGC_COMP_H_

#include <unordered_map>
#include <vector>

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

class CompleteMissionsId;

namespace common
{
enum eCondtionType 
{
    E_CONDITION_KILL_MONSTER = 1,
    E_CONDITION_TALK_WITH_NPC ,
    E_CONDITION_COMPLELETE_CONDITION,
    E_CONDITION_MAX,
};

enum eConditionIndex
{
    E_CONDITION_1,
    E_CONDITION_2
};

enum eConditionStatus
{
    E_CONDITION_NORMAL,
    E_CONDITION_COMPLETE,
};

enum eMissionStatus
{
    E_MISSION_NORMAL,
    E_MISSION_ACHIEVEMENT,
    E_MISSION_COMPLETE,
    E_MISSION_TIME_OUT,
    E_MISSION_FAILD,
    E_MISSION_MAX,
};

using EntityV = std::vector<entt::entity>;
using TypeMissionIdMap = std::unordered_map<uint32_t, UI32USet>;
using MissionAutoRewardCallback = std::function<bool(uint32_t)>;
using CompleteMissionCallback = std::function<void(entt::entity, uint32_t, CompleteMissionsId&)>;
}//namespace common

#endif//COMMON_SRC_GAME_LOIGC_COMP_H_
