#pragma once
#include <cstdint>

#include "proto/scene/game_player.pb.h"


constexpr uint32_t GamePlayerServiceCentre2GsLoginMessageId = 83;
constexpr uint32_t GamePlayerServiceCentre2GsLoginIndex = 0;
#define GamePlayerServiceCentre2GsLoginMethod  ::GamePlayerService_Stub::descriptor()->method(0)

constexpr uint32_t GamePlayerServiceExitGameMessageId = 35;
constexpr uint32_t GamePlayerServiceExitGameIndex = 1;
#define GamePlayerServiceExitGameMethod  ::GamePlayerService_Stub::descriptor()->method(1)


