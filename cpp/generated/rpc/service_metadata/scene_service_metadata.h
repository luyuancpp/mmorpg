#pragma once
#include <cstdint>

#include "proto/service/cpp/rpc/scene/scene.pb.h"


constexpr uint32_t ScenePlayerEnterGameNodeMessageId = 83;
constexpr uint32_t ScenePlayerEnterGameNodeIndex = 0;
#define ScenePlayerEnterGameNodeMethod  ::Scene_Stub::descriptor()->method(0)

constexpr uint32_t SceneSendMessageToPlayerMessageId = 1;
constexpr uint32_t SceneSendMessageToPlayerIndex = 1;
#define SceneSendMessageToPlayerMethod  ::Scene_Stub::descriptor()->method(1)

constexpr uint32_t SceneClientSendMessageToPlayerMessageId = 10;
constexpr uint32_t SceneClientSendMessageToPlayerIndex = 2;
#define SceneClientSendMessageToPlayerMethod  ::Scene_Stub::descriptor()->method(2)

constexpr uint32_t SceneCentreSendToPlayerViaGameNodeMessageId = 57;
constexpr uint32_t SceneCentreSendToPlayerViaGameNodeIndex = 3;
#define SceneCentreSendToPlayerViaGameNodeMethod  ::Scene_Stub::descriptor()->method(3)

constexpr uint32_t SceneInvokePlayerServiceMessageId = 40;
constexpr uint32_t SceneInvokePlayerServiceIndex = 4;
#define SceneInvokePlayerServiceMethod  ::Scene_Stub::descriptor()->method(4)

constexpr uint32_t SceneRouteNodeStringMsgMessageId = 78;
constexpr uint32_t SceneRouteNodeStringMsgIndex = 5;
#define SceneRouteNodeStringMsgMethod  ::Scene_Stub::descriptor()->method(5)

constexpr uint32_t SceneRoutePlayerStringMsgMessageId = 81;
constexpr uint32_t SceneRoutePlayerStringMsgIndex = 6;
#define SceneRoutePlayerStringMsgMethod  ::Scene_Stub::descriptor()->method(6)

constexpr uint32_t SceneUpdateSessionDetailMessageId = 24;
constexpr uint32_t SceneUpdateSessionDetailIndex = 7;
#define SceneUpdateSessionDetailMethod  ::Scene_Stub::descriptor()->method(7)

constexpr uint32_t SceneEnterSceneMessageId = 71;
constexpr uint32_t SceneEnterSceneIndex = 8;
#define SceneEnterSceneMethod  ::Scene_Stub::descriptor()->method(8)

constexpr uint32_t SceneCreateSceneMessageId = 36;
constexpr uint32_t SceneCreateSceneIndex = 9;
#define SceneCreateSceneMethod  ::Scene_Stub::descriptor()->method(9)

constexpr uint32_t SceneNodeHandshakeMessageId = 62;
constexpr uint32_t SceneNodeHandshakeIndex = 10;
#define SceneNodeHandshakeMethod  ::Scene_Stub::descriptor()->method(10)


