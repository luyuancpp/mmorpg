#pragma once
#include <cstdint>

#include "proto/scene/player_scene.pb.h"


constexpr uint32_t SceneSceneClientPlayerEnterSceneMessageId = 24;
constexpr uint32_t SceneSceneClientPlayerEnterSceneIndex = 0;
#define SceneSceneClientPlayerEnterSceneMethod  ::SceneSceneClientPlayer_Stub::descriptor()->method(0)

constexpr uint32_t SceneSceneClientPlayerNotifyEnterSceneMessageId = 64;
constexpr uint32_t SceneSceneClientPlayerNotifyEnterSceneIndex = 1;
#define SceneSceneClientPlayerNotifyEnterSceneMethod  ::SceneSceneClientPlayer_Stub::descriptor()->method(1)

constexpr uint32_t SceneSceneClientPlayerSceneInfoC2SMessageId = 37;
constexpr uint32_t SceneSceneClientPlayerSceneInfoC2SIndex = 2;
#define SceneSceneClientPlayerSceneInfoC2SMethod  ::SceneSceneClientPlayer_Stub::descriptor()->method(2)

constexpr uint32_t SceneSceneClientPlayerNotifySceneInfoMessageId = 83;
constexpr uint32_t SceneSceneClientPlayerNotifySceneInfoIndex = 3;
#define SceneSceneClientPlayerNotifySceneInfoMethod  ::SceneSceneClientPlayer_Stub::descriptor()->method(3)

constexpr uint32_t SceneSceneClientPlayerNotifyActorCreateMessageId = 66;
constexpr uint32_t SceneSceneClientPlayerNotifyActorCreateIndex = 4;
#define SceneSceneClientPlayerNotifyActorCreateMethod  ::SceneSceneClientPlayer_Stub::descriptor()->method(4)

constexpr uint32_t SceneSceneClientPlayerNotifyActorDestroyMessageId = 49;
constexpr uint32_t SceneSceneClientPlayerNotifyActorDestroyIndex = 5;
#define SceneSceneClientPlayerNotifyActorDestroyMethod  ::SceneSceneClientPlayer_Stub::descriptor()->method(5)

constexpr uint32_t SceneSceneClientPlayerNotifyActorListCreateMessageId = 25;
constexpr uint32_t SceneSceneClientPlayerNotifyActorListCreateIndex = 6;
#define SceneSceneClientPlayerNotifyActorListCreateMethod  ::SceneSceneClientPlayer_Stub::descriptor()->method(6)

constexpr uint32_t SceneSceneClientPlayerNotifyActorListDestroyMessageId = 27;
constexpr uint32_t SceneSceneClientPlayerNotifyActorListDestroyIndex = 7;
#define SceneSceneClientPlayerNotifyActorListDestroyMethod  ::SceneSceneClientPlayer_Stub::descriptor()->method(7)


