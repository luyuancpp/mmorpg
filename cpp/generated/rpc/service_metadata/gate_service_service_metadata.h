#pragma once
#include <cstdint>

#include "proto/gate/gate_service.pb.h"

constexpr uint32_t GatePlayerEnterGameNodeMessageId = 42;
constexpr uint32_t GatePlayerEnterGameNodeIndex = 0;
#define GatePlayerEnterGameNodeMethod  ::Gate_Stub::descriptor()->method(0)

constexpr uint32_t GateSendMessageToPlayerMessageId = 72;
constexpr uint32_t GateSendMessageToPlayerIndex = 1;
#define GateSendMessageToPlayerMethod  ::Gate_Stub::descriptor()->method(1)

constexpr uint32_t GateRouteNodeMessageMessageId = 41;
constexpr uint32_t GateRouteNodeMessageIndex = 2;
#define GateRouteNodeMessageMethod  ::Gate_Stub::descriptor()->method(2)

constexpr uint32_t GateRoutePlayerMessageMessageId = 74;
constexpr uint32_t GateRoutePlayerMessageIndex = 3;
#define GateRoutePlayerMessageMethod  ::Gate_Stub::descriptor()->method(3)

constexpr uint32_t GateBroadcastToPlayersMessageId = 32;
constexpr uint32_t GateBroadcastToPlayersIndex = 4;
#define GateBroadcastToPlayersMethod  ::Gate_Stub::descriptor()->method(4)

constexpr uint32_t GateBroadcastToSceneMessageId = 57;
constexpr uint32_t GateBroadcastToSceneIndex = 5;
#define GateBroadcastToSceneMethod  ::Gate_Stub::descriptor()->method(5)

constexpr uint32_t GateBroadcastToAllMessageId = 71;
constexpr uint32_t GateBroadcastToAllIndex = 6;
#define GateBroadcastToAllMethod  ::Gate_Stub::descriptor()->method(6)

constexpr uint32_t GateNodeHandshakeMessageId = 45;
constexpr uint32_t GateNodeHandshakeIndex = 7;
#define GateNodeHandshakeMethod  ::Gate_Stub::descriptor()->method(7)

constexpr uint32_t GateBindSessionToGateMessageId = 56;
constexpr uint32_t GateBindSessionToGateIndex = 8;
#define GateBindSessionToGateMethod  ::Gate_Stub::descriptor()->method(8)

constexpr uint32_t GateGmGracefulShutdownMessageId = 125;
constexpr uint32_t GateGmGracefulShutdownIndex = 9;
#define GateGmGracefulShutdownMethod  ::Gate_Stub::descriptor()->method(9)
