#pragma once
#include <cstdint>

#include "proto/service/cpp/rpc/scene/player_state_attribute_sync.pb.h"


constexpr uint32_t ScenePlayerSyncSyncBaseAttributeMessageId = 66;
constexpr uint32_t ScenePlayerSyncSyncBaseAttributeIndex = 0;
#define ScenePlayerSyncSyncBaseAttributeMethod  ::ScenePlayerSync_Stub::descriptor()->method(0)

constexpr uint32_t ScenePlayerSyncSyncAttribute2FramesMessageId = 65;
constexpr uint32_t ScenePlayerSyncSyncAttribute2FramesIndex = 1;
#define ScenePlayerSyncSyncAttribute2FramesMethod  ::ScenePlayerSync_Stub::descriptor()->method(1)

constexpr uint32_t ScenePlayerSyncSyncAttribute5FramesMessageId = 55;
constexpr uint32_t ScenePlayerSyncSyncAttribute5FramesIndex = 2;
#define ScenePlayerSyncSyncAttribute5FramesMethod  ::ScenePlayerSync_Stub::descriptor()->method(2)

constexpr uint32_t ScenePlayerSyncSyncAttribute10FramesMessageId = 82;
constexpr uint32_t ScenePlayerSyncSyncAttribute10FramesIndex = 3;
#define ScenePlayerSyncSyncAttribute10FramesMethod  ::ScenePlayerSync_Stub::descriptor()->method(3)

constexpr uint32_t ScenePlayerSyncSyncAttribute30FramesMessageId = 68;
constexpr uint32_t ScenePlayerSyncSyncAttribute30FramesIndex = 4;
#define ScenePlayerSyncSyncAttribute30FramesMethod  ::ScenePlayerSync_Stub::descriptor()->method(4)

constexpr uint32_t ScenePlayerSyncSyncAttribute60FramesMessageId = 75;
constexpr uint32_t ScenePlayerSyncSyncAttribute60FramesIndex = 5;
#define ScenePlayerSyncSyncAttribute60FramesMethod  ::ScenePlayerSync_Stub::descriptor()->method(5)


