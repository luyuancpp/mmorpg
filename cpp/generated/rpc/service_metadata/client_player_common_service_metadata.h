#pragma once
#include <cstdint>

#include "proto/scene/client_player_common.pb.h"

constexpr uint32_t SceneClientPlayerCommonSendTipToClientMessageId = 23;
constexpr uint32_t SceneClientPlayerCommonSendTipToClientIndex = 0;
#define SceneClientPlayerCommonSendTipToClientMethod  ::SceneClientPlayerCommon_Stub::descriptor()->method(0)

constexpr uint32_t SceneClientPlayerCommonKickPlayerMessageId = 34;
constexpr uint32_t SceneClientPlayerCommonKickPlayerIndex = 1;
#define SceneClientPlayerCommonKickPlayerMethod  ::SceneClientPlayerCommon_Stub::descriptor()->method(1)

constexpr uint32_t SceneClientPlayerCommonRedirectToGateMessageId = 124;
constexpr uint32_t SceneClientPlayerCommonRedirectToGateIndex = 2;
#define SceneClientPlayerCommonRedirectToGateMethod  ::SceneClientPlayerCommon_Stub::descriptor()->method(2)
