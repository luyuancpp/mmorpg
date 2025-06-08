#pragma once
#include <cstdint>

#include "proto/centre/centre_client_player.pb.h"


constexpr uint32_t CentrePlayerUtilitySendTipToClientMessageId = 10;
constexpr uint32_t CentrePlayerUtilitySendTipToClientIndex = 0;
#define CentrePlayerUtilitySendTipToClientMethod  ::CentrePlayerUtility_Stub::descriptor()->method(0)

constexpr uint32_t CentrePlayerUtilityKickPlayerMessageId = 33;
constexpr uint32_t CentrePlayerUtilityKickPlayerIndex = 1;
#define CentrePlayerUtilityKickPlayerMethod  ::CentrePlayerUtility_Stub::descriptor()->method(1)


