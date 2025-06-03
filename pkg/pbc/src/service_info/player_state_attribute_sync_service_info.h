#pragma once
#include <cstdint>

#include "proto/scene/player_state_attribute_sync.pb.h"


constexpr uint32_t EntitySyncServiceSyncBaseAttributeMessageId = 41;
constexpr uint32_t EntitySyncServiceSyncBaseAttributeIndex = 0;
#define EntitySyncServiceSyncBaseAttributeMethod  ::EntitySyncService_Stub::descriptor()->method(0)

constexpr uint32_t EntitySyncServiceSyncAttribute2FramesMessageId = 58;
constexpr uint32_t EntitySyncServiceSyncAttribute2FramesIndex = 1;
#define EntitySyncServiceSyncAttribute2FramesMethod  ::EntitySyncService_Stub::descriptor()->method(1)

constexpr uint32_t EntitySyncServiceSyncAttribute5FramesMessageId = 50;
constexpr uint32_t EntitySyncServiceSyncAttribute5FramesIndex = 2;
#define EntitySyncServiceSyncAttribute5FramesMethod  ::EntitySyncService_Stub::descriptor()->method(2)

constexpr uint32_t EntitySyncServiceSyncAttribute10FramesMessageId = 32;
constexpr uint32_t EntitySyncServiceSyncAttribute10FramesIndex = 3;
#define EntitySyncServiceSyncAttribute10FramesMethod  ::EntitySyncService_Stub::descriptor()->method(3)

constexpr uint32_t EntitySyncServiceSyncAttribute30FramesMessageId = 54;
constexpr uint32_t EntitySyncServiceSyncAttribute30FramesIndex = 4;
#define EntitySyncServiceSyncAttribute30FramesMethod  ::EntitySyncService_Stub::descriptor()->method(4)

constexpr uint32_t EntitySyncServiceSyncAttribute60FramesMessageId = 79;
constexpr uint32_t EntitySyncServiceSyncAttribute60FramesIndex = 5;
#define EntitySyncServiceSyncAttribute60FramesMethod  ::EntitySyncService_Stub::descriptor()->method(5)


