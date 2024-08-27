#pragma once
#include <cstdint>

#include "logic/server_player/centre_player.pb.h"

constexpr uint32_t CentrePlayerServiceTestMessageId = 6;
constexpr uint32_t CentrePlayerServiceTestIndex = 0;
#define CentrePlayerServiceTestMethod  ::CentrePlayerService_Stub::descriptor()->method(0)

