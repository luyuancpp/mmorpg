#pragma once
#include <cstdint>

#include "proto/common/gate_service.pb.h"


constexpr uint32_t GateServicePlayerEnterGameNodeMessageId = 3;
constexpr uint32_t GateServicePlayerEnterGameNodeIndex = 0;
#define GateServicePlayerEnterGameNodeMethod  ::GateService_Stub::descriptor()->method(0)

constexpr uint32_t GateServiceSendMessageToPlayerMessageId = 72;
constexpr uint32_t GateServiceSendMessageToPlayerIndex = 1;
#define GateServiceSendMessageToPlayerMethod  ::GateService_Stub::descriptor()->method(1)

constexpr uint32_t GateServiceKickSessionByCentreMessageId = 22;
constexpr uint32_t GateServiceKickSessionByCentreIndex = 2;
#define GateServiceKickSessionByCentreMethod  ::GateService_Stub::descriptor()->method(2)

constexpr uint32_t GateServiceRouteNodeMessageMessageId = 65;
constexpr uint32_t GateServiceRouteNodeMessageIndex = 3;
#define GateServiceRouteNodeMessageMethod  ::GateService_Stub::descriptor()->method(3)

constexpr uint32_t GateServiceRoutePlayerMessageMessageId = 78;
constexpr uint32_t GateServiceRoutePlayerMessageIndex = 4;
#define GateServiceRoutePlayerMessageMethod  ::GateService_Stub::descriptor()->method(4)

constexpr uint32_t GateServiceBroadcastToPlayersMessageId = 32;
constexpr uint32_t GateServiceBroadcastToPlayersIndex = 5;
#define GateServiceBroadcastToPlayersMethod  ::GateService_Stub::descriptor()->method(5)

constexpr uint32_t GateServiceRegisterNodeSessionMessageId = 44;
constexpr uint32_t GateServiceRegisterNodeSessionIndex = 6;
#define GateServiceRegisterNodeSessionMethod  ::GateService_Stub::descriptor()->method(6)


