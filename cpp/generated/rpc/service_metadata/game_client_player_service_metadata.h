#pragma once
#include <cstdint>

#include "proto/service/cpp/rpc/scene/game_client_player.pb.h"


constexpr uint32_t SceneClientPlayerCommonSendTipToClientMessageId = 54;
constexpr uint32_t SceneClientPlayerCommonSendTipToClientIndex = 0;
#define SceneClientPlayerCommonSendTipToClientMethod  ::SceneClientPlayerCommon_Stub::descriptor()->method(0)

constexpr uint32_t SceneClientPlayerCommonKickPlayerMessageId = 63;
constexpr uint32_t SceneClientPlayerCommonKickPlayerIndex = 1;
#define SceneClientPlayerCommonKickPlayerMethod  ::SceneClientPlayerCommon_Stub::descriptor()->method(1)


