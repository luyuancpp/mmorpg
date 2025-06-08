#pragma once
#include <cstdint>

#include "proto/centre/centre_player.pb.h"


constexpr uint32_t CentrePlayerUtilitySendTipToClientMessageId = 18446744073709551615;
constexpr uint32_t CentrePlayerUtilitySendTipToClientIndex = 0;
#define CentrePlayerUtilitySendTipToClientMethod  ::CentrePlayerUtility_Stub::descriptor()->method(0)

constexpr uint32_t CentrePlayerUtilityKickPlayerMessageId = 18446744073709551615;
constexpr uint32_t CentrePlayerUtilityKickPlayerIndex = 1;
#define CentrePlayerUtilityKickPlayerMethod  ::CentrePlayerUtility_Stub::descriptor()->method(1)


