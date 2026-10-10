#pragma once
#include <cstdint>

#include "proto/scene/player_bag.pb.h"

constexpr uint32_t SceneBagClientPlayerGetBagMessageId = 191;
constexpr uint32_t SceneBagClientPlayerGetBagIndex = 0;
#define SceneBagClientPlayerGetBagMethod  ::SceneBagClientPlayer_Stub::descriptor()->method(0)

constexpr uint32_t SceneBagClientPlayerSortBagMessageId = 192;
constexpr uint32_t SceneBagClientPlayerSortBagIndex = 1;
#define SceneBagClientPlayerSortBagMethod  ::SceneBagClientPlayer_Stub::descriptor()->method(1)

constexpr uint32_t SceneBagClientPlayerEquipItemMessageId = 249;
constexpr uint32_t SceneBagClientPlayerEquipItemIndex = 2;
#define SceneBagClientPlayerEquipItemMethod  ::SceneBagClientPlayer_Stub::descriptor()->method(2)

constexpr uint32_t SceneBagClientPlayerUnequipItemMessageId = 246;
constexpr uint32_t SceneBagClientPlayerUnequipItemIndex = 3;
#define SceneBagClientPlayerUnequipItemMethod  ::SceneBagClientPlayer_Stub::descriptor()->method(3)

constexpr uint32_t SceneBagClientPlayerGmGrantItemMessageId = 247;
constexpr uint32_t SceneBagClientPlayerGmGrantItemIndex = 4;
#define SceneBagClientPlayerGmGrantItemMethod  ::SceneBagClientPlayer_Stub::descriptor()->method(4)
