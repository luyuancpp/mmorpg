#pragma once
#include <cstdint>

#include "proto/scene/game_service.pb.h"


constexpr uint32_t GameServicePlayerEnterGameNodeMessageId = 25;
constexpr uint32_t GameServicePlayerEnterGameNodeIndex = 0;
#define GameServicePlayerEnterGameNodeMethod  ::GameService_Stub::descriptor()->method(0)

constexpr uint32_t GameServiceSendMessageToPlayerMessageId = 27;
constexpr uint32_t GameServiceSendMessageToPlayerIndex = 1;
#define GameServiceSendMessageToPlayerMethod  ::GameService_Stub::descriptor()->method(1)

constexpr uint32_t GameServiceClientSendMessageToPlayerMessageId = 5;
constexpr uint32_t GameServiceClientSendMessageToPlayerIndex = 2;
#define GameServiceClientSendMessageToPlayerMethod  ::GameService_Stub::descriptor()->method(2)

constexpr uint32_t GameServiceCentreSendToPlayerViaGameNodeMessageId = 45;
constexpr uint32_t GameServiceCentreSendToPlayerViaGameNodeIndex = 3;
#define GameServiceCentreSendToPlayerViaGameNodeMethod  ::GameService_Stub::descriptor()->method(3)

constexpr uint32_t GameServiceInvokePlayerServiceMessageId = 29;
constexpr uint32_t GameServiceInvokePlayerServiceIndex = 4;
#define GameServiceInvokePlayerServiceMethod  ::GameService_Stub::descriptor()->method(4)

constexpr uint32_t GameServiceRouteNodeStringMsgMessageId = 40;
constexpr uint32_t GameServiceRouteNodeStringMsgIndex = 5;
#define GameServiceRouteNodeStringMsgMethod  ::GameService_Stub::descriptor()->method(5)

constexpr uint32_t GameServiceRoutePlayerStringMsgMessageId = 51;
constexpr uint32_t GameServiceRoutePlayerStringMsgIndex = 6;
#define GameServiceRoutePlayerStringMsgMethod  ::GameService_Stub::descriptor()->method(6)

constexpr uint32_t GameServiceUpdateSessionDetailMessageId = 18;
constexpr uint32_t GameServiceUpdateSessionDetailIndex = 7;
#define GameServiceUpdateSessionDetailMethod  ::GameService_Stub::descriptor()->method(7)

constexpr uint32_t GameServiceEnterSceneMessageId = 49;
constexpr uint32_t GameServiceEnterSceneIndex = 8;
#define GameServiceEnterSceneMethod  ::GameService_Stub::descriptor()->method(8)

constexpr uint32_t GameServiceCreateSceneMessageId = 65;
constexpr uint32_t GameServiceCreateSceneIndex = 9;
#define GameServiceCreateSceneMethod  ::GameService_Stub::descriptor()->method(9)

constexpr uint32_t GameServiceRegisterNodeSessionMessageId = 4;
constexpr uint32_t GameServiceRegisterNodeSessionIndex = 10;
#define GameServiceRegisterNodeSessionMethod  ::GameService_Stub::descriptor()->method(10)


