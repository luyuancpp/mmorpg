#pragma once
#include <cstdint>

#include "proto/game/game_client_player.pb.h"


constexpr uint32_t GameClientPlayerCommonServiceSendTipToClientMessageId = 83;
constexpr uint32_t GameClientPlayerCommonServiceSendTipToClientIndex = 0;
#define GameClientPlayerCommonServiceSendTipToClientMethod  ::GameClientPlayerCommonService_Stub::descriptor()->method(0)

constexpr uint32_t GameClientPlayerCommonServiceKickPlayerMessageId = 87;
constexpr uint32_t GameClientPlayerCommonServiceKickPlayerIndex = 1;
#define GameClientPlayerCommonServiceKickPlayerMethod  ::GameClientPlayerCommonService_Stub::descriptor()->method(1)


