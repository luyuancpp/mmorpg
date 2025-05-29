#pragma once
#include <cstdint>

#include "proto/logic/server_player/game_player.pb.h"


constexpr uint32_t GamePlayerServiceCentre2GsLoginMessageId = 20;
constexpr uint32_t GamePlayerServiceCentre2GsLoginIndex = 0;
#define GamePlayerServiceCentre2GsLoginMethod  ::GamePlayerService_Stub::descriptor()->method(0)

constexpr uint32_t GamePlayerServiceExitGameMessageId = 62;
constexpr uint32_t GamePlayerServiceExitGameIndex = 1;
#define GamePlayerServiceExitGameMethod  ::GamePlayerService_Stub::descriptor()->method(1)


