#pragma once
#include <cstdint>

#include "proto/scene/player_lifecycle.pb.h"

constexpr uint32_t ScenePlayerGateLoginNotifyMessageId = 50;
constexpr uint32_t ScenePlayerGateLoginNotifyIndex = 0;
#define ScenePlayerGateLoginNotifyMethod  ::ScenePlayer_Stub::descriptor()->method(0)

constexpr uint32_t ScenePlayerExitGameMessageId = 9;
constexpr uint32_t ScenePlayerExitGameIndex = 1;
#define ScenePlayerExitGameMethod  ::ScenePlayer_Stub::descriptor()->method(1)
