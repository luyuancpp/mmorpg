#pragma once

#include <unordered_map>
#include <vector>

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

enum eCondtionType : uint32_t
{
    E_CONDITION_KILL_MONSTER = 1,
    E_CONDITION_TALK_WITH_NPC,
    E_CONDITION_COMPLELETE_CONDITION,
    E_CONDITION_USE_ITEM,
    E_CONDITION_INTERATION,
    E_CONDITION_LEVEUP,
    E_CONDITION_COMSTUM,
    E_CONDITION_COMPLELTE_MISSION,
    E_CONDITION_MAX,
};

enum eConditionIndex
{
    E_CONDITION_1,
    E_CONDITION_2
};

using MissionAutoRewardCallback = std::function<bool(uint32_t)>;
using TempCompleteList = UInt32Set;
using TypeSubTypeSet = UInt32PairSet;

struct NextTimeAcceptMission { UInt32Set next_time_accept_mission_id_; };
struct CheckSubType {};
struct MissionReward {};


