#pragma once
#include <cstdint>

#include "logic/client_player/player_state_attribute_sync.pb.h"

constexpr uint32_t EntitySyncServiceSyncBaseAttributeMessageId = 66;
constexpr uint32_t EntitySyncServiceSyncBaseAttributeIndex = 0;
#define EntitySyncServiceSyncBaseAttributeMethod  ::EntitySyncService_Stub::descriptor()->method(0)

constexpr uint32_t EntitySyncServiceSyncAttributeMessageId = 67;
constexpr uint32_t EntitySyncServiceSyncAttributeIndex = 1;
#define EntitySyncServiceSyncAttributeMethod  ::EntitySyncService_Stub::descriptor()->method(1)

