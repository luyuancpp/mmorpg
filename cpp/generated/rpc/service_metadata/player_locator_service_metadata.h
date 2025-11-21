#pragma once
#include <cstdint>

#include "proto/service/go/grpc/player_locator/player_locator.pb.h"


constexpr uint32_t PlayerLocatorSetLocationMessageId = 13;
constexpr uint32_t PlayerLocatorSetLocationIndex = 0;
#define PlayerLocatorSetLocationMethod  ::PlayerLocator_Stub::descriptor()->method(0)

constexpr uint32_t PlayerLocatorGetLocationMessageId = 64;
constexpr uint32_t PlayerLocatorGetLocationIndex = 1;
#define PlayerLocatorGetLocationMethod  ::PlayerLocator_Stub::descriptor()->method(1)

constexpr uint32_t PlayerLocatorMarkOfflineMessageId = 18;
constexpr uint32_t PlayerLocatorMarkOfflineIndex = 2;
#define PlayerLocatorMarkOfflineMethod  ::PlayerLocator_Stub::descriptor()->method(2)


