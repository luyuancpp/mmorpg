#pragma once
#include <cstdint>

#include "proto/scene/game_service.pb.h"


constexpr uint32_t ScenePlayerEnterGameNodeMessageId = 52;
constexpr uint32_t ScenePlayerEnterGameNodeIndex = 0;
#define ScenePlayerEnterGameNodeMethod  ::Scene_Stub::descriptor()->method(0)

constexpr uint32_t SceneSendMessageToPlayerMessageId = 59;
constexpr uint32_t SceneSendMessageToPlayerIndex = 1;
#define SceneSendMessageToPlayerMethod  ::Scene_Stub::descriptor()->method(1)

constexpr uint32_t SceneClientSendMessageToPlayerMessageId = 16;
constexpr uint32_t SceneClientSendMessageToPlayerIndex = 2;
#define SceneClientSendMessageToPlayerMethod  ::Scene_Stub::descriptor()->method(2)

constexpr uint32_t SceneCentreSendToPlayerViaGameNodeMessageId = 20;
constexpr uint32_t SceneCentreSendToPlayerViaGameNodeIndex = 3;
#define SceneCentreSendToPlayerViaGameNodeMethod  ::Scene_Stub::descriptor()->method(3)

constexpr uint32_t SceneInvokePlayerServiceMessageId = 7;
constexpr uint32_t SceneInvokePlayerServiceIndex = 4;
#define SceneInvokePlayerServiceMethod  ::Scene_Stub::descriptor()->method(4)

constexpr uint32_t SceneRouteNodeStringMsgMessageId = 82;
constexpr uint32_t SceneRouteNodeStringMsgIndex = 5;
#define SceneRouteNodeStringMsgMethod  ::Scene_Stub::descriptor()->method(5)

constexpr uint32_t SceneRoutePlayerStringMsgMessageId = 12;
constexpr uint32_t SceneRoutePlayerStringMsgIndex = 6;
#define SceneRoutePlayerStringMsgMethod  ::Scene_Stub::descriptor()->method(6)

constexpr uint32_t SceneUpdateSessionDetailMessageId = 22;
constexpr uint32_t SceneUpdateSessionDetailIndex = 7;
#define SceneUpdateSessionDetailMethod  ::Scene_Stub::descriptor()->method(7)

constexpr uint32_t SceneEnterSceneMessageId = 72;
constexpr uint32_t SceneEnterSceneIndex = 8;
#define SceneEnterSceneMethod  ::Scene_Stub::descriptor()->method(8)

constexpr uint32_t SceneCreateSceneMessageId = 30;
constexpr uint32_t SceneCreateSceneIndex = 9;
#define SceneCreateSceneMethod  ::Scene_Stub::descriptor()->method(9)

constexpr uint32_t SceneRegisterNodeSessionMessageId = 26;
constexpr uint32_t SceneRegisterNodeSessionIndex = 10;
#define SceneRegisterNodeSessionMethod  ::Scene_Stub::descriptor()->method(10)


