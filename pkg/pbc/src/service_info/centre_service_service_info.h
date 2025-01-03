#pragma once
#include <cstdint>

#include "proto/common/centre_service.pb.h"

constexpr uint32_t CentreServiceRegisterGameNodeMessageId = 54;
constexpr uint32_t CentreServiceRegisterGameNodeIndex = 0;
#define CentreServiceRegisterGameNodeMethod  ::CentreService_Stub::descriptor()->method(0)

constexpr uint32_t CentreServiceRegisterGateNodeMessageId = 2;
constexpr uint32_t CentreServiceRegisterGateNodeIndex = 1;
#define CentreServiceRegisterGateNodeMethod  ::CentreService_Stub::descriptor()->method(1)

constexpr uint32_t CentreServiceGatePlayerServiceMessageId = 36;
constexpr uint32_t CentreServiceGatePlayerServiceIndex = 2;
#define CentreServiceGatePlayerServiceMethod  ::CentreService_Stub::descriptor()->method(2)

constexpr uint32_t CentreServiceGateSessionDisconnectMessageId = 9;
constexpr uint32_t CentreServiceGateSessionDisconnectIndex = 3;
#define CentreServiceGateSessionDisconnectMethod  ::CentreService_Stub::descriptor()->method(3)

constexpr uint32_t CentreServiceLoginNodeAccountLoginMessageId = 19;
constexpr uint32_t CentreServiceLoginNodeAccountLoginIndex = 4;
#define CentreServiceLoginNodeAccountLoginMethod  ::CentreService_Stub::descriptor()->method(4)

constexpr uint32_t CentreServiceLoginNodeEnterGameMessageId = 53;
constexpr uint32_t CentreServiceLoginNodeEnterGameIndex = 5;
#define CentreServiceLoginNodeEnterGameMethod  ::CentreService_Stub::descriptor()->method(5)

constexpr uint32_t CentreServiceLoginNodeLeaveGameMessageId = 41;
constexpr uint32_t CentreServiceLoginNodeLeaveGameIndex = 6;
#define CentreServiceLoginNodeLeaveGameMethod  ::CentreService_Stub::descriptor()->method(6)

constexpr uint32_t CentreServiceLoginNodeSessionDisconnectMessageId = 24;
constexpr uint32_t CentreServiceLoginNodeSessionDisconnectIndex = 7;
#define CentreServiceLoginNodeSessionDisconnectMethod  ::CentreService_Stub::descriptor()->method(7)

constexpr uint32_t CentreServicePlayerServiceMessageId = 28;
constexpr uint32_t CentreServicePlayerServiceIndex = 8;
#define CentreServicePlayerServiceMethod  ::CentreService_Stub::descriptor()->method(8)

constexpr uint32_t CentreServiceEnterGsSucceedMessageId = 38;
constexpr uint32_t CentreServiceEnterGsSucceedIndex = 9;
#define CentreServiceEnterGsSucceedMethod  ::CentreService_Stub::descriptor()->method(9)

constexpr uint32_t CentreServiceRouteNodeStringMsgMessageId = 42;
constexpr uint32_t CentreServiceRouteNodeStringMsgIndex = 10;
#define CentreServiceRouteNodeStringMsgMethod  ::CentreService_Stub::descriptor()->method(10)

constexpr uint32_t CentreServiceRoutePlayerStringMsgMessageId = 45;
constexpr uint32_t CentreServiceRoutePlayerStringMsgIndex = 11;
#define CentreServiceRoutePlayerStringMsgMethod  ::CentreService_Stub::descriptor()->method(11)

constexpr uint32_t CentreServiceUnRegisterGameNodeMessageId = 31;
constexpr uint32_t CentreServiceUnRegisterGameNodeIndex = 12;
#define CentreServiceUnRegisterGameNodeMethod  ::CentreService_Stub::descriptor()->method(12)

