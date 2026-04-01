#pragma once
#include <cstdint>

#include "proto/slg/slg_map.pb.h"

constexpr uint32_t SlgMapStartMarchMessageId = 122;
constexpr uint32_t SlgMapStartMarchIndex = 0;
#define SlgMapStartMarchMethod  ::SlgMap_Stub::descriptor()->method(0)

constexpr uint32_t SlgMapCancelMarchMessageId = 123;
constexpr uint32_t SlgMapCancelMarchIndex = 1;
#define SlgMapCancelMarchMethod  ::SlgMap_Stub::descriptor()->method(1)

constexpr uint32_t SlgMapQueryViewportMessageId = 119;
constexpr uint32_t SlgMapQueryViewportIndex = 2;
#define SlgMapQueryViewportMethod  ::SlgMap_Stub::descriptor()->method(2)

constexpr uint32_t SlgMapBuildMessageId = 121;
constexpr uint32_t SlgMapBuildIndex = 3;
#define SlgMapBuildMethod  ::SlgMap_Stub::descriptor()->method(3)

constexpr uint32_t SlgMapUpgradeBuildingMessageId = 125;
constexpr uint32_t SlgMapUpgradeBuildingIndex = 4;
#define SlgMapUpgradeBuildingMethod  ::SlgMap_Stub::descriptor()->method(4)

constexpr uint32_t SlgMapNodeHandshakeMessageId = 120;
constexpr uint32_t SlgMapNodeHandshakeIndex = 5;
#define SlgMapNodeHandshakeMethod  ::SlgMap_Stub::descriptor()->method(5)
