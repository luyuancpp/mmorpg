#pragma once
#include <cstdint>

#include "proto/gate/gate_service.pb.h"


constexpr uint32_t GatePlayerEnterGameNodeMessageId = 35;
constexpr uint32_t GatePlayerEnterGameNodeIndex = 0;
#define GatePlayerEnterGameNodeMethod  ::Gate_Stub::descriptor()->method(0)

constexpr uint32_t GateSendMessageToPlayerMessageId = 0;
constexpr uint32_t GateSendMessageToPlayerIndex = 1;
#define GateSendMessageToPlayerMethod  ::Gate_Stub::descriptor()->method(1)

constexpr uint32_t GateKickSessionByCentreMessageId = 11;
constexpr uint32_t GateKickSessionByCentreIndex = 2;
#define GateKickSessionByCentreMethod  ::Gate_Stub::descriptor()->method(2)

constexpr uint32_t GateRouteNodeMessageMessageId = 18;
constexpr uint32_t GateRouteNodeMessageIndex = 3;
#define GateRouteNodeMessageMethod  ::Gate_Stub::descriptor()->method(3)

constexpr uint32_t GateRoutePlayerMessageMessageId = 12;
constexpr uint32_t GateRoutePlayerMessageIndex = 4;
#define GateRoutePlayerMessageMethod  ::Gate_Stub::descriptor()->method(4)

constexpr uint32_t GateBroadcastToPlayersMessageId = 23;
constexpr uint32_t GateBroadcastToPlayersIndex = 5;
#define GateBroadcastToPlayersMethod  ::Gate_Stub::descriptor()->method(5)

constexpr uint32_t GateRegisterNodeSessionMessageId = 19;
constexpr uint32_t GateRegisterNodeSessionIndex = 6;
#define GateRegisterNodeSessionMethod  ::Gate_Stub::descriptor()->method(6)


