#pragma once
#include <cstdint>

#include "proto/centre/centre_service.pb.h"


constexpr uint32_t CentreServiceGatePlayerServiceMessageId = 17;
constexpr uint32_t CentreServiceGatePlayerServiceIndex = 0;
#define CentreServiceGatePlayerServiceMethod  ::CentreService_Stub::descriptor()->method(0)

constexpr uint32_t CentreServiceGateSessionDisconnectMessageId = 50;
constexpr uint32_t CentreServiceGateSessionDisconnectIndex = 1;
#define CentreServiceGateSessionDisconnectMethod  ::CentreService_Stub::descriptor()->method(1)

constexpr uint32_t CentreServiceLoginNodeAccountLoginMessageId = 4;
constexpr uint32_t CentreServiceLoginNodeAccountLoginIndex = 2;
#define CentreServiceLoginNodeAccountLoginMethod  ::CentreService_Stub::descriptor()->method(2)

constexpr uint32_t CentreServiceLoginNodeEnterGameMessageId = 5;
constexpr uint32_t CentreServiceLoginNodeEnterGameIndex = 3;
#define CentreServiceLoginNodeEnterGameMethod  ::CentreService_Stub::descriptor()->method(3)

constexpr uint32_t CentreServiceLoginNodeLeaveGameMessageId = 36;
constexpr uint32_t CentreServiceLoginNodeLeaveGameIndex = 4;
#define CentreServiceLoginNodeLeaveGameMethod  ::CentreService_Stub::descriptor()->method(4)

constexpr uint32_t CentreServiceLoginNodeSessionDisconnectMessageId = 9;
constexpr uint32_t CentreServiceLoginNodeSessionDisconnectIndex = 5;
#define CentreServiceLoginNodeSessionDisconnectMethod  ::CentreService_Stub::descriptor()->method(5)

constexpr uint32_t CentreServicePlayerServiceMessageId = 80;
constexpr uint32_t CentreServicePlayerServiceIndex = 6;
#define CentreServicePlayerServiceMethod  ::CentreService_Stub::descriptor()->method(6)

constexpr uint32_t CentreServiceEnterGsSucceedMessageId = 19;
constexpr uint32_t CentreServiceEnterGsSucceedIndex = 7;
#define CentreServiceEnterGsSucceedMethod  ::CentreService_Stub::descriptor()->method(7)

constexpr uint32_t CentreServiceRouteNodeStringMsgMessageId = 49;
constexpr uint32_t CentreServiceRouteNodeStringMsgIndex = 8;
#define CentreServiceRouteNodeStringMsgMethod  ::CentreService_Stub::descriptor()->method(8)

constexpr uint32_t CentreServiceRoutePlayerStringMsgMessageId = 15;
constexpr uint32_t CentreServiceRoutePlayerStringMsgIndex = 9;
#define CentreServiceRoutePlayerStringMsgMethod  ::CentreService_Stub::descriptor()->method(9)

constexpr uint32_t CentreServiceInitSceneNodeMessageId = 3;
constexpr uint32_t CentreServiceInitSceneNodeIndex = 10;
#define CentreServiceInitSceneNodeMethod  ::CentreService_Stub::descriptor()->method(10)

constexpr uint32_t CentreServiceRegisterNodeSessionMessageId = 2;
constexpr uint32_t CentreServiceRegisterNodeSessionIndex = 11;
#define CentreServiceRegisterNodeSessionMethod  ::CentreService_Stub::descriptor()->method(11)


