#pragma once
#include <cstdint>

#include "proto/service/rpc/centre/centre_player.pb.h"


constexpr uint32_t CentrePlayerUtilitySendTipToClientMessageId = 48;
constexpr uint32_t CentrePlayerUtilitySendTipToClientIndex = 0;
#define CentrePlayerUtilitySendTipToClientMethod  ::CentrePlayerUtility_Stub::descriptor()->method(0)

constexpr uint32_t CentrePlayerUtilityKickPlayerMessageId = 12;
constexpr uint32_t CentrePlayerUtilityKickPlayerIndex = 1;
#define CentrePlayerUtilityKickPlayerMethod  ::CentrePlayerUtility_Stub::descriptor()->method(1)


