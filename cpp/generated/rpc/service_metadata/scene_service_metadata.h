#pragma once
#include <cstdint>

#include "proto/scene/scene.pb.h"


constexpr uint32_t ScenePlayerEnterGameNodeMessageId = 38;
constexpr uint32_t ScenePlayerEnterGameNodeIndex = 0;
#define ScenePlayerEnterGameNodeMethod  ::Scene_Stub::descriptor()->method(0)

constexpr uint32_t SceneSendMessageToPlayerMessageId = 60;
constexpr uint32_t SceneSendMessageToPlayerIndex = 1;
#define SceneSendMessageToPlayerMethod  ::Scene_Stub::descriptor()->method(1)

constexpr uint32_t SceneClientSendMessageToPlayerMessageId = 6;
constexpr uint32_t SceneClientSendMessageToPlayerIndex = 2;
#define SceneClientSendMessageToPlayerMethod  ::Scene_Stub::descriptor()->method(2)

constexpr uint32_t SceneCentreSendToPlayerViaGameNodeMessageId = 74;
constexpr uint32_t SceneCentreSendToPlayerViaGameNodeIndex = 3;
#define SceneCentreSendToPlayerViaGameNodeMethod  ::Scene_Stub::descriptor()->method(3)

constexpr uint32_t SceneInvokePlayerServiceMessageId = 59;
constexpr uint32_t SceneInvokePlayerServiceIndex = 4;
#define SceneInvokePlayerServiceMethod  ::Scene_Stub::descriptor()->method(4)

constexpr uint32_t SceneRouteNodeStringMsgMessageId = 41;
constexpr uint32_t SceneRouteNodeStringMsgIndex = 5;
#define SceneRouteNodeStringMsgMethod  ::Scene_Stub::descriptor()->method(5)

constexpr uint32_t SceneRoutePlayerStringMsgMessageId = 30;
constexpr uint32_t SceneRoutePlayerStringMsgIndex = 6;
#define SceneRoutePlayerStringMsgMethod  ::Scene_Stub::descriptor()->method(6)

constexpr uint32_t SceneUpdateSessionDetailMessageId = 7;
constexpr uint32_t SceneUpdateSessionDetailIndex = 7;
#define SceneUpdateSessionDetailMethod  ::Scene_Stub::descriptor()->method(7)

constexpr uint32_t SceneEnterSceneMessageId = 28;
constexpr uint32_t SceneEnterSceneIndex = 8;
#define SceneEnterSceneMethod  ::Scene_Stub::descriptor()->method(8)

constexpr uint32_t SceneCreateSceneMessageId = 79;
constexpr uint32_t SceneCreateSceneIndex = 9;
#define SceneCreateSceneMethod  ::Scene_Stub::descriptor()->method(9)

constexpr uint32_t SceneNodeHandshakeMessageId = 23;
constexpr uint32_t SceneNodeHandshakeIndex = 10;
#define SceneNodeHandshakeMethod  ::Scene_Stub::descriptor()->method(10)


