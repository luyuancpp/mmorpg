#pragma once
#include <cstdint>

#include "logic/client_player/player_state_attribute_sync.pb.h"

constexpr uint32_t EntityStateSyncServiceSyncVelocityMessageId = 66;
constexpr uint32_t EntityStateSyncServiceSyncVelocityIndex = 0;
#define EntityStateSyncServiceSyncVelocityMethod  ::EntityStateSyncService_Stub::descriptor()->method(0)

