#pragma once
#include <cstdint>

#include "proto/scene/game_player_scene.pb.h"


constexpr uint32_t GamePlayerSceneServiceEnterSceneMessageId = 84;
constexpr uint32_t GamePlayerSceneServiceEnterSceneIndex = 0;
#define GamePlayerSceneServiceEnterSceneMethod  ::GamePlayerSceneService_Stub::descriptor()->method(0)

constexpr uint32_t GamePlayerSceneServiceLeaveSceneMessageId = 53;
constexpr uint32_t GamePlayerSceneServiceLeaveSceneIndex = 1;
#define GamePlayerSceneServiceLeaveSceneMethod  ::GamePlayerSceneService_Stub::descriptor()->method(1)

constexpr uint32_t GamePlayerSceneServiceEnterSceneS2CMessageId = 12;
constexpr uint32_t GamePlayerSceneServiceEnterSceneS2CIndex = 2;
#define GamePlayerSceneServiceEnterSceneS2CMethod  ::GamePlayerSceneService_Stub::descriptor()->method(2)


