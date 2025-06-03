#pragma once
#include <cstdint>

#include "proto/centre/centre_client_player.pb.h"


constexpr uint32_t CentreClientPlayerCommonServiceSendTipToClientMessageId = 34;
constexpr uint32_t CentreClientPlayerCommonServiceSendTipToClientIndex = 0;
#define CentreClientPlayerCommonServiceSendTipToClientMethod  ::CentreClientPlayerCommonService_Stub::descriptor()->method(0)

constexpr uint32_t CentreClientPlayerCommonServiceKickPlayerMessageId = 37;
constexpr uint32_t CentreClientPlayerCommonServiceKickPlayerIndex = 1;
#define CentreClientPlayerCommonServiceKickPlayerMethod  ::CentreClientPlayerCommonService_Stub::descriptor()->method(1)


