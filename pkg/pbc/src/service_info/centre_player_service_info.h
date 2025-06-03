#pragma once
#include <cstdint>

#include "proto/centre/centre_player.pb.h"


constexpr uint32_t CentrePlayerServiceTestMessageId = 66;
constexpr uint32_t CentrePlayerServiceTestIndex = 0;
#define CentrePlayerServiceTestMethod  ::CentrePlayerService_Stub::descriptor()->method(0)


