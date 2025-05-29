#pragma once
#include <cstdint>

#include "proto/logic/client_player/player_scene.pb.h"


constexpr uint32_t ClientPlayerSceneServiceEnterSceneMessageId = 68;
constexpr uint32_t ClientPlayerSceneServiceEnterSceneIndex = 0;
#define ClientPlayerSceneServiceEnterSceneMethod  ::ClientPlayerSceneService_Stub::descriptor()->method(0)

constexpr uint32_t ClientPlayerSceneServiceNotifyEnterSceneMessageId = 63;
constexpr uint32_t ClientPlayerSceneServiceNotifyEnterSceneIndex = 1;
#define ClientPlayerSceneServiceNotifyEnterSceneMethod  ::ClientPlayerSceneService_Stub::descriptor()->method(1)

constexpr uint32_t ClientPlayerSceneServiceSceneInfoC2SMessageId = 77;
constexpr uint32_t ClientPlayerSceneServiceSceneInfoC2SIndex = 2;
#define ClientPlayerSceneServiceSceneInfoC2SMethod  ::ClientPlayerSceneService_Stub::descriptor()->method(2)

constexpr uint32_t ClientPlayerSceneServiceNotifySceneInfoMessageId = 82;
constexpr uint32_t ClientPlayerSceneServiceNotifySceneInfoIndex = 3;
#define ClientPlayerSceneServiceNotifySceneInfoMethod  ::ClientPlayerSceneService_Stub::descriptor()->method(3)

constexpr uint32_t ClientPlayerSceneServiceNotifyActorCreateMessageId = 38;
constexpr uint32_t ClientPlayerSceneServiceNotifyActorCreateIndex = 4;
#define ClientPlayerSceneServiceNotifyActorCreateMethod  ::ClientPlayerSceneService_Stub::descriptor()->method(4)

constexpr uint32_t ClientPlayerSceneServiceNotifyActorDestroyMessageId = 25;
constexpr uint32_t ClientPlayerSceneServiceNotifyActorDestroyIndex = 5;
#define ClientPlayerSceneServiceNotifyActorDestroyMethod  ::ClientPlayerSceneService_Stub::descriptor()->method(5)

constexpr uint32_t ClientPlayerSceneServiceNotifyActorListCreateMessageId = 41;
constexpr uint32_t ClientPlayerSceneServiceNotifyActorListCreateIndex = 6;
#define ClientPlayerSceneServiceNotifyActorListCreateMethod  ::ClientPlayerSceneService_Stub::descriptor()->method(6)

constexpr uint32_t ClientPlayerSceneServiceNotifyActorListDestroyMessageId = 48;
constexpr uint32_t ClientPlayerSceneServiceNotifyActorListDestroyIndex = 7;
#define ClientPlayerSceneServiceNotifyActorListDestroyMethod  ::ClientPlayerSceneService_Stub::descriptor()->method(7)


