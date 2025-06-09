#pragma once
#include <cstdint>

#include "proto/scene/game_service.pb.h"


constexpr uint32_t ScenePlayerEnterGameNodeMessageId = 67;
constexpr uint32_t ScenePlayerEnterGameNodeIndex = 0;
#define ScenePlayerEnterGameNodeMethod  ::Scene_Stub::descriptor()->method(0)

constexpr uint32_t SceneSendMessageToPlayerMessageId = 76;
constexpr uint32_t SceneSendMessageToPlayerIndex = 1;
#define SceneSendMessageToPlayerMethod  ::Scene_Stub::descriptor()->method(1)

constexpr uint32_t SceneClientSendMessageToPlayerMessageId = 79;
constexpr uint32_t SceneClientSendMessageToPlayerIndex = 2;
#define SceneClientSendMessageToPlayerMethod  ::Scene_Stub::descriptor()->method(2)

constexpr uint32_t SceneCentreSendToPlayerViaGameNodeMessageId = 55;
constexpr uint32_t SceneCentreSendToPlayerViaGameNodeIndex = 3;
#define SceneCentreSendToPlayerViaGameNodeMethod  ::Scene_Stub::descriptor()->method(3)

constexpr uint32_t SceneInvokePlayerServiceMessageId = 62;
constexpr uint32_t SceneInvokePlayerServiceIndex = 4;
#define SceneInvokePlayerServiceMethod  ::Scene_Stub::descriptor()->method(4)

constexpr uint32_t SceneRouteNodeStringMsgMessageId = 65;
constexpr uint32_t SceneRouteNodeStringMsgIndex = 5;
#define SceneRouteNodeStringMsgMethod  ::Scene_Stub::descriptor()->method(5)

constexpr uint32_t SceneRoutePlayerStringMsgMessageId = 83;
constexpr uint32_t SceneRoutePlayerStringMsgIndex = 6;
#define SceneRoutePlayerStringMsgMethod  ::Scene_Stub::descriptor()->method(6)

constexpr uint32_t SceneUpdateSessionDetailMessageId = 60;
constexpr uint32_t SceneUpdateSessionDetailIndex = 7;
#define SceneUpdateSessionDetailMethod  ::Scene_Stub::descriptor()->method(7)

constexpr uint32_t SceneEnterSceneMessageId = 81;
constexpr uint32_t SceneEnterSceneIndex = 8;
#define SceneEnterSceneMethod  ::Scene_Stub::descriptor()->method(8)

constexpr uint32_t SceneCreateSceneMessageId = 64;
constexpr uint32_t SceneCreateSceneIndex = 9;
#define SceneCreateSceneMethod  ::Scene_Stub::descriptor()->method(9)

constexpr uint32_t SceneRegisterNodeSessionMessageId = 68;
constexpr uint32_t SceneRegisterNodeSessionIndex = 10;
#define SceneRegisterNodeSessionMethod  ::Scene_Stub::descriptor()->method(10)


