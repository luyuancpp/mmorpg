#pragma once
#include <cstdint>

#include "logic/client_player/player_state_attribute_sync.pb.h"

constexpr uint32_t EntityStateSyncServiceSyncBaseStateAttributeMessageId = 67;
constexpr uint32_t EntityStateSyncServiceSyncBaseStateAttributeIndex = 0;
#define EntityStateSyncServiceSyncBaseStateAttributeMethod  ::EntityStateSyncService_Stub::descriptor()->method(0)

