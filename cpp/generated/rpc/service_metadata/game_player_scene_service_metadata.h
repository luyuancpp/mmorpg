#pragma once
#include <cstdint>

#include "proto/service/cpp/rpc/scene/game_player_scene.pb.h"


constexpr uint32_t SceneScenePlayerEnterSceneMessageId = 70;
constexpr uint32_t SceneScenePlayerEnterSceneIndex = 0;
#define SceneScenePlayerEnterSceneMethod  ::SceneScenePlayer_Stub::descriptor()->method(0)

constexpr uint32_t SceneScenePlayerLeaveSceneMessageId = 32;
constexpr uint32_t SceneScenePlayerLeaveSceneIndex = 1;
#define SceneScenePlayerLeaveSceneMethod  ::SceneScenePlayer_Stub::descriptor()->method(1)

constexpr uint32_t SceneScenePlayerEnterSceneS2CMessageId = 29;
constexpr uint32_t SceneScenePlayerEnterSceneS2CIndex = 2;
#define SceneScenePlayerEnterSceneS2CMethod  ::SceneScenePlayer_Stub::descriptor()->method(2)


