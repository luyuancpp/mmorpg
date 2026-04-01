#pragma once
#include <cstdint>

#include "proto/turnbased/turnbased_battle.pb.h"

constexpr uint32_t TurnBasedBattleStartBattleMessageId = 127;
constexpr uint32_t TurnBasedBattleStartBattleIndex = 0;
#define TurnBasedBattleStartBattleMethod  ::TurnBasedBattle_Stub::descriptor()->method(0)

constexpr uint32_t TurnBasedBattleExecuteActionMessageId = 128;
constexpr uint32_t TurnBasedBattleExecuteActionIndex = 1;
#define TurnBasedBattleExecuteActionMethod  ::TurnBasedBattle_Stub::descriptor()->method(1)

constexpr uint32_t TurnBasedBattleQueryBattleMessageId = 129;
constexpr uint32_t TurnBasedBattleQueryBattleIndex = 2;
#define TurnBasedBattleQueryBattleMethod  ::TurnBasedBattle_Stub::descriptor()->method(2)

constexpr uint32_t TurnBasedBattleNodeHandshakeMessageId = 132;
constexpr uint32_t TurnBasedBattleNodeHandshakeIndex = 3;
#define TurnBasedBattleNodeHandshakeMethod  ::TurnBasedBattle_Stub::descriptor()->method(3)
