#pragma once
#include <cstdint>

#include "proto/service/cpp/rpc/scene/game_player.pb.h"


constexpr uint32_t ScenePlayerCentre2GsLoginMessageId = 35;
constexpr uint32_t ScenePlayerCentre2GsLoginIndex = 0;
#define ScenePlayerCentre2GsLoginMethod  ::ScenePlayer_Stub::descriptor()->method(0)

constexpr uint32_t ScenePlayerExitGameMessageId = 43;
constexpr uint32_t ScenePlayerExitGameIndex = 1;
#define ScenePlayerExitGameMethod  ::ScenePlayer_Stub::descriptor()->method(1)


