#pragma once
#include <cstdint>

#include "proto/scene/game_player.pb.h"


constexpr uint32_t ScenePlayerCentre2GsLoginMessageId = 15;
constexpr uint32_t ScenePlayerCentre2GsLoginIndex = 0;
#define ScenePlayerCentre2GsLoginMethod  ::ScenePlayer_Stub::descriptor()->method(0)

constexpr uint32_t ScenePlayerExitGameMessageId = 19;
constexpr uint32_t ScenePlayerExitGameIndex = 1;
#define ScenePlayerExitGameMethod  ::ScenePlayer_Stub::descriptor()->method(1)


