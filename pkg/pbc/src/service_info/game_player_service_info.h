#pragma once
#include <cstdint>

#include "proto/game/game_player.pb.h"


constexpr uint32_t GamePlayerServiceCentre2GsLoginMessageId = 79;
constexpr uint32_t GamePlayerServiceCentre2GsLoginIndex = 0;
#define GamePlayerServiceCentre2GsLoginMethod  ::GamePlayerService_Stub::descriptor()->method(0)

constexpr uint32_t GamePlayerServiceExitGameMessageId = 26;
constexpr uint32_t GamePlayerServiceExitGameIndex = 1;
#define GamePlayerServiceExitGameMethod  ::GamePlayerService_Stub::descriptor()->method(1)


