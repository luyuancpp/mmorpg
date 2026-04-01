#pragma once
#include <cstdint>

#include "proto/turnbased/turnbased_dungeon.pb.h"

constexpr uint32_t TurnBasedDungeonEnterDungeonMessageId = 130;
constexpr uint32_t TurnBasedDungeonEnterDungeonIndex = 0;
#define TurnBasedDungeonEnterDungeonMethod  ::TurnBasedDungeon_Stub::descriptor()->method(0)

constexpr uint32_t TurnBasedDungeonMoveInDungeonMessageId = 131;
constexpr uint32_t TurnBasedDungeonMoveInDungeonIndex = 1;
#define TurnBasedDungeonMoveInDungeonMethod  ::TurnBasedDungeon_Stub::descriptor()->method(1)

constexpr uint32_t TurnBasedDungeonNextFloorMessageId = 134;
constexpr uint32_t TurnBasedDungeonNextFloorIndex = 2;
#define TurnBasedDungeonNextFloorMethod  ::TurnBasedDungeon_Stub::descriptor()->method(2)

constexpr uint32_t TurnBasedDungeonQueryDungeonMessageId = 135;
constexpr uint32_t TurnBasedDungeonQueryDungeonIndex = 3;
#define TurnBasedDungeonQueryDungeonMethod  ::TurnBasedDungeon_Stub::descriptor()->method(3)

constexpr uint32_t TurnBasedDungeonLeaveDungeonMessageId = 136;
constexpr uint32_t TurnBasedDungeonLeaveDungeonIndex = 4;
#define TurnBasedDungeonLeaveDungeonMethod  ::TurnBasedDungeon_Stub::descriptor()->method(4)

constexpr uint32_t TurnBasedDungeonNodeHandshakeMessageId = 133;
constexpr uint32_t TurnBasedDungeonNodeHandshakeIndex = 5;
#define TurnBasedDungeonNodeHandshakeMethod  ::TurnBasedDungeon_Stub::descriptor()->method(5)
