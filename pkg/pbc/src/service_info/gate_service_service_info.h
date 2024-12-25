#pragma once
#include <cstdint>

#include "common/gate_service.pb.h"

constexpr uint32_t GateServiceRegisterGameMessageId = 13;
constexpr uint32_t GateServiceRegisterGameIndex = 0;
#define GateServiceRegisterGameMethod  ::GateService_Stub::descriptor()->method(0)

constexpr uint32_t GateServiceUnRegisterGameMessageId = 40;
constexpr uint32_t GateServiceUnRegisterGameIndex = 1;
#define GateServiceUnRegisterGameMethod  ::GateService_Stub::descriptor()->method(1)

constexpr uint32_t GateServicePlayerEnterGameNodeMessageId = 11;
constexpr uint32_t GateServicePlayerEnterGameNodeIndex = 2;
#define GateServicePlayerEnterGameNodeMethod  ::GateService_Stub::descriptor()->method(2)

constexpr uint32_t GateServiceSendMessageToPlayerMessageId = 63;
constexpr uint32_t GateServiceSendMessageToPlayerIndex = 3;
#define GateServiceSendMessageToPlayerMethod  ::GateService_Stub::descriptor()->method(3)

constexpr uint32_t GateServiceKickSessionByCentreMessageId = 30;
constexpr uint32_t GateServiceKickSessionByCentreIndex = 4;
#define GateServiceKickSessionByCentreMethod  ::GateService_Stub::descriptor()->method(4)

constexpr uint32_t GateServiceRouteNodeMessageMessageId = 12;
constexpr uint32_t GateServiceRouteNodeMessageIndex = 5;
#define GateServiceRouteNodeMessageMethod  ::GateService_Stub::descriptor()->method(5)

constexpr uint32_t GateServiceRoutePlayerMessageMessageId = 15;
constexpr uint32_t GateServiceRoutePlayerMessageIndex = 6;
#define GateServiceRoutePlayerMessageMethod  ::GateService_Stub::descriptor()->method(6)

constexpr uint32_t GateServiceBroadcastToPlayersMessageId = 14;
constexpr uint32_t GateServiceBroadcastToPlayersIndex = 7;
#define GateServiceBroadcastToPlayersMethod  ::GateService_Stub::descriptor()->method(7)

