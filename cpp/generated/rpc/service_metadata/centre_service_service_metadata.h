#pragma once
#include <cstdint>

#include "proto/service/cpp/rpc/centre/centre_service.pb.h"


constexpr uint32_t CentreGatePlayerServiceMessageId = 23;
constexpr uint32_t CentreGatePlayerServiceIndex = 0;
#define CentreGatePlayerServiceMethod  ::Centre_Stub::descriptor()->method(0)

constexpr uint32_t CentreGateSessionDisconnectMessageId = 79;
constexpr uint32_t CentreGateSessionDisconnectIndex = 1;
#define CentreGateSessionDisconnectMethod  ::Centre_Stub::descriptor()->method(1)

constexpr uint32_t CentreLoginNodeAccountLoginMessageId = 65;
constexpr uint32_t CentreLoginNodeAccountLoginIndex = 2;
#define CentreLoginNodeAccountLoginMethod  ::Centre_Stub::descriptor()->method(2)

constexpr uint32_t CentreLoginNodeEnterGameMessageId = 47;
constexpr uint32_t CentreLoginNodeEnterGameIndex = 3;
#define CentreLoginNodeEnterGameMethod  ::Centre_Stub::descriptor()->method(3)

constexpr uint32_t CentreLoginNodeLeaveGameMessageId = 19;
constexpr uint32_t CentreLoginNodeLeaveGameIndex = 4;
#define CentreLoginNodeLeaveGameMethod  ::Centre_Stub::descriptor()->method(4)

constexpr uint32_t CentreLoginNodeSessionDisconnectMessageId = 6;
constexpr uint32_t CentreLoginNodeSessionDisconnectIndex = 5;
#define CentreLoginNodeSessionDisconnectMethod  ::Centre_Stub::descriptor()->method(5)

constexpr uint32_t CentrePlayerServiceMessageId = 82;
constexpr uint32_t CentrePlayerServiceIndex = 6;
#define CentrePlayerServiceMethod  ::Centre_Stub::descriptor()->method(6)

constexpr uint32_t CentreEnterGsSucceedMessageId = 58;
constexpr uint32_t CentreEnterGsSucceedIndex = 7;
#define CentreEnterGsSucceedMethod  ::Centre_Stub::descriptor()->method(7)

constexpr uint32_t CentreRouteNodeStringMsgMessageId = 42;
constexpr uint32_t CentreRouteNodeStringMsgIndex = 8;
#define CentreRouteNodeStringMsgMethod  ::Centre_Stub::descriptor()->method(8)

constexpr uint32_t CentreRoutePlayerStringMsgMessageId = 31;
constexpr uint32_t CentreRoutePlayerStringMsgIndex = 9;
#define CentreRoutePlayerStringMsgMethod  ::Centre_Stub::descriptor()->method(9)

constexpr uint32_t CentreInitSceneNodeMessageId = 34;
constexpr uint32_t CentreInitSceneNodeIndex = 10;
#define CentreInitSceneNodeMethod  ::Centre_Stub::descriptor()->method(10)

constexpr uint32_t CentreNodeHandshakeMessageId = 46;
constexpr uint32_t CentreNodeHandshakeIndex = 11;
#define CentreNodeHandshakeMethod  ::Centre_Stub::descriptor()->method(11)


