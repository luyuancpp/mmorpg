#pragma once
#include <cstdint>

#include "proto/common/game_service.pb.h"

constexpr uint32_t GameServicePlayerEnterGameNodeMessageId = 7;
constexpr uint32_t GameServicePlayerEnterGameNodeIndex = 0;
#define GameServicePlayerEnterGameNodeMethod  ::GameService_Stub::descriptor()->method(0)

constexpr uint32_t GameServiceSendMessageToPlayerMessageId = 35;
constexpr uint32_t GameServiceSendMessageToPlayerIndex = 1;
#define GameServiceSendMessageToPlayerMethod  ::GameService_Stub::descriptor()->method(1)

constexpr uint32_t GameServiceClientSendMessageToPlayerMessageId = 26;
constexpr uint32_t GameServiceClientSendMessageToPlayerIndex = 2;
#define GameServiceClientSendMessageToPlayerMethod  ::GameService_Stub::descriptor()->method(2)

constexpr uint32_t GameServiceRegisterGateNodeMessageId = 61;
constexpr uint32_t GameServiceRegisterGateNodeIndex = 3;
#define GameServiceRegisterGateNodeMethod  ::GameService_Stub::descriptor()->method(3)

constexpr uint32_t GameServiceCentreSendToPlayerViaGameNodeMessageId = 25;
constexpr uint32_t GameServiceCentreSendToPlayerViaGameNodeIndex = 4;
#define GameServiceCentreSendToPlayerViaGameNodeMethod  ::GameService_Stub::descriptor()->method(4)

constexpr uint32_t GameServiceInvokePlayerServiceMessageId = 59;
constexpr uint32_t GameServiceInvokePlayerServiceIndex = 5;
#define GameServiceInvokePlayerServiceMethod  ::GameService_Stub::descriptor()->method(5)

constexpr uint32_t GameServiceRouteNodeStringMsgMessageId = 64;
constexpr uint32_t GameServiceRouteNodeStringMsgIndex = 6;
#define GameServiceRouteNodeStringMsgMethod  ::GameService_Stub::descriptor()->method(6)

constexpr uint32_t GameServiceRoutePlayerStringMsgMessageId = 65;
constexpr uint32_t GameServiceRoutePlayerStringMsgIndex = 7;
#define GameServiceRoutePlayerStringMsgMethod  ::GameService_Stub::descriptor()->method(7)

constexpr uint32_t GameServiceUpdateSessionDetailMessageId = 39;
constexpr uint32_t GameServiceUpdateSessionDetailIndex = 8;
#define GameServiceUpdateSessionDetailMethod  ::GameService_Stub::descriptor()->method(8)

constexpr uint32_t GameServiceEnterSceneMessageId = 75;
constexpr uint32_t GameServiceEnterSceneIndex = 9;
#define GameServiceEnterSceneMethod  ::GameService_Stub::descriptor()->method(9)

constexpr uint32_t GameServiceCreateSceneMessageId = 44;
constexpr uint32_t GameServiceCreateSceneIndex = 10;
#define GameServiceCreateSceneMethod  ::GameService_Stub::descriptor()->method(10)
