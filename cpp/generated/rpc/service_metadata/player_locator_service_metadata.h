#pragma once
#include <cstdint>

#include "proto/player_locator/player_locator.pb.h"

constexpr uint32_t PlayerLocatorSetLocationMessageId = 307;
constexpr uint32_t PlayerLocatorSetLocationIndex = 0;
#define PlayerLocatorSetLocationMethod  ::PlayerLocator_Stub::descriptor()->method(0)

constexpr uint32_t PlayerLocatorGetLocationMessageId = 308;
constexpr uint32_t PlayerLocatorGetLocationIndex = 1;
#define PlayerLocatorGetLocationMethod  ::PlayerLocator_Stub::descriptor()->method(1)

constexpr uint32_t PlayerLocatorMarkOfflineMessageId = 309;
constexpr uint32_t PlayerLocatorMarkOfflineIndex = 2;
#define PlayerLocatorMarkOfflineMethod  ::PlayerLocator_Stub::descriptor()->method(2)

constexpr uint32_t PlayerLocatorSetSessionMessageId = 310;
constexpr uint32_t PlayerLocatorSetSessionIndex = 3;
#define PlayerLocatorSetSessionMethod  ::PlayerLocator_Stub::descriptor()->method(3)

constexpr uint32_t PlayerLocatorGetSessionMessageId = 311;
constexpr uint32_t PlayerLocatorGetSessionIndex = 4;
#define PlayerLocatorGetSessionMethod  ::PlayerLocator_Stub::descriptor()->method(4)

constexpr uint32_t PlayerLocatorSetDisconnectingMessageId = 312;
constexpr uint32_t PlayerLocatorSetDisconnectingIndex = 5;
#define PlayerLocatorSetDisconnectingMethod  ::PlayerLocator_Stub::descriptor()->method(5)

constexpr uint32_t PlayerLocatorReconnectMessageId = 313;
constexpr uint32_t PlayerLocatorReconnectIndex = 6;
#define PlayerLocatorReconnectMethod  ::PlayerLocator_Stub::descriptor()->method(6)
