#pragma once
#include <cstdint>

#include "proto/slg/slg_battle.pb.h"

constexpr uint32_t SlgBattleSimulateBattleMessageId = 126;
constexpr uint32_t SlgBattleSimulateBattleIndex = 0;
#define SlgBattleSimulateBattleMethod  ::SlgBattle_Stub::descriptor()->method(0)

constexpr uint32_t SlgBattleQueryBattleReportMessageId = 124;
constexpr uint32_t SlgBattleQueryBattleReportIndex = 1;
#define SlgBattleQueryBattleReportMethod  ::SlgBattle_Stub::descriptor()->method(1)
