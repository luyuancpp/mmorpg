#pragma once
#include <cstdint>

#include "proto/logic/client_player/player_common.pb.h"


constexpr uint32_t PlayerClientCommonServiceSendTipToClientMessageId = 0;
constexpr uint32_t PlayerClientCommonServiceSendTipToClientIndex = 0;
#define PlayerClientCommonServiceSendTipToClientMethod  ::PlayerClientCommonService_Stub::descriptor()->method(0)

constexpr uint32_t PlayerClientCommonServiceKickPlayerMessageId = 37;
constexpr uint32_t PlayerClientCommonServiceKickPlayerIndex = 1;
#define PlayerClientCommonServiceKickPlayerMethod  ::PlayerClientCommonService_Stub::descriptor()->method(1)


