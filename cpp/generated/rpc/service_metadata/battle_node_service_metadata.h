#pragma once
#include <cstdint>

#include "proto/battle/battle_node.pb.h"

constexpr uint32_t BattleNodeQueryBattleCreationMessageId = 283;
constexpr uint32_t BattleNodeQueryBattleCreationIndex = 0;
#define BattleNodeQueryBattleCreationMethod  ::BattleNode_Stub::descriptor()->method(0)

constexpr uint32_t BattleNodeCreateBattleMessageId = 146;
constexpr uint32_t BattleNodeCreateBattleIndex = 1;
#define BattleNodeCreateBattleMethod  ::BattleNode_Stub::descriptor()->method(1)

constexpr uint32_t BattleNodeDestroyBattleMessageId = 147;
constexpr uint32_t BattleNodeDestroyBattleIndex = 2;
#define BattleNodeDestroyBattleMethod  ::BattleNode_Stub::descriptor()->method(2)

constexpr uint32_t BattleNodeAddObserverMessageId = 160;
constexpr uint32_t BattleNodeAddObserverIndex = 3;
#define BattleNodeAddObserverMethod  ::BattleNode_Stub::descriptor()->method(3)

constexpr uint32_t BattleNodeRemoveObserverMessageId = 159;
constexpr uint32_t BattleNodeRemoveObserverIndex = 4;
#define BattleNodeRemoveObserverMethod  ::BattleNode_Stub::descriptor()->method(4)

constexpr uint32_t BattleNodeIssueBattleTicketMessageId = 178;
constexpr uint32_t BattleNodeIssueBattleTicketIndex = 5;
#define BattleNodeIssueBattleTicketMethod  ::BattleNode_Stub::descriptor()->method(5)
