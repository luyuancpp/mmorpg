#pragma once
#include <cstdint>

#include "proto/scene/player_state_attribute_sync.pb.h"


constexpr uint32_t PlayerSyncServiceSyncBaseAttributeMessageId = 39;
constexpr uint32_t PlayerSyncServiceSyncBaseAttributeIndex = 0;
#define PlayerSyncServiceSyncBaseAttributeMethod  ::PlayerSyncService_Stub::descriptor()->method(0)

constexpr uint32_t PlayerSyncServiceSyncAttribute2FramesMessageId = 20;
constexpr uint32_t PlayerSyncServiceSyncAttribute2FramesIndex = 1;
#define PlayerSyncServiceSyncAttribute2FramesMethod  ::PlayerSyncService_Stub::descriptor()->method(1)

constexpr uint32_t PlayerSyncServiceSyncAttribute5FramesMessageId = 23;
constexpr uint32_t PlayerSyncServiceSyncAttribute5FramesIndex = 2;
#define PlayerSyncServiceSyncAttribute5FramesMethod  ::PlayerSyncService_Stub::descriptor()->method(2)

constexpr uint32_t PlayerSyncServiceSyncAttribute10FramesMessageId = 37;
constexpr uint32_t PlayerSyncServiceSyncAttribute10FramesIndex = 3;
#define PlayerSyncServiceSyncAttribute10FramesMethod  ::PlayerSyncService_Stub::descriptor()->method(3)

constexpr uint32_t PlayerSyncServiceSyncAttribute30FramesMessageId = 79;
constexpr uint32_t PlayerSyncServiceSyncAttribute30FramesIndex = 4;
#define PlayerSyncServiceSyncAttribute30FramesMethod  ::PlayerSyncService_Stub::descriptor()->method(4)

constexpr uint32_t PlayerSyncServiceSyncAttribute60FramesMessageId = 81;
constexpr uint32_t PlayerSyncServiceSyncAttribute60FramesIndex = 5;
#define PlayerSyncServiceSyncAttribute60FramesMethod  ::PlayerSyncService_Stub::descriptor()->method(5)


