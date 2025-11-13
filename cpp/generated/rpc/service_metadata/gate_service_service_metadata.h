#pragma once
#include <cstdint>

#include "proto/service/cpp/rpc/gate/gate_service.pb.h"


constexpr uint32_t GatePlayerEnterGameNodeMessageId = 69;
constexpr uint32_t GatePlayerEnterGameNodeIndex = 0;
#define GatePlayerEnterGameNodeMethod  ::Gate_Stub::descriptor()->method(0)

constexpr uint32_t GateSendMessageToPlayerMessageId = 20;
constexpr uint32_t GateSendMessageToPlayerIndex = 1;
#define GateSendMessageToPlayerMethod  ::Gate_Stub::descriptor()->method(1)

constexpr uint32_t GateKickSessionByCentreMessageId = 48;
constexpr uint32_t GateKickSessionByCentreIndex = 2;
#define GateKickSessionByCentreMethod  ::Gate_Stub::descriptor()->method(2)

constexpr uint32_t GateRouteNodeMessageMessageId = 53;
constexpr uint32_t GateRouteNodeMessageIndex = 3;
#define GateRouteNodeMessageMethod  ::Gate_Stub::descriptor()->method(3)

constexpr uint32_t GateRoutePlayerMessageMessageId = 8;
constexpr uint32_t GateRoutePlayerMessageIndex = 4;
#define GateRoutePlayerMessageMethod  ::Gate_Stub::descriptor()->method(4)

constexpr uint32_t GateBroadcastToPlayersMessageId = 76;
constexpr uint32_t GateBroadcastToPlayersIndex = 5;
#define GateBroadcastToPlayersMethod  ::Gate_Stub::descriptor()->method(5)

constexpr uint32_t GateNodeHandshakeMessageId = 72;
constexpr uint32_t GateNodeHandshakeIndex = 6;
#define GateNodeHandshakeMethod  ::Gate_Stub::descriptor()->method(6)

constexpr uint32_t GateBindSessionToGateMessageId = 84;
constexpr uint32_t GateBindSessionToGateIndex = 7;
#define GateBindSessionToGateMethod  ::Gate_Stub::descriptor()->method(7)


