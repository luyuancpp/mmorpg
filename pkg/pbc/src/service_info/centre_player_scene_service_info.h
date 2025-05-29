#pragma once
#include <cstdint>

#include "proto/logic/server_player/centre_player_scene.pb.h"


constexpr uint32_t CentrePlayerSceneServiceEnterSceneMessageId = 56;
constexpr uint32_t CentrePlayerSceneServiceEnterSceneIndex = 0;
#define CentrePlayerSceneServiceEnterSceneMethod  ::CentrePlayerSceneService_Stub::descriptor()->method(0)

constexpr uint32_t CentrePlayerSceneServiceLeaveSceneMessageId = 14;
constexpr uint32_t CentrePlayerSceneServiceLeaveSceneIndex = 1;
#define CentrePlayerSceneServiceLeaveSceneMethod  ::CentrePlayerSceneService_Stub::descriptor()->method(1)

constexpr uint32_t CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId = 21;
constexpr uint32_t CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteIndex = 2;
#define CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMethod  ::CentrePlayerSceneService_Stub::descriptor()->method(2)

constexpr uint32_t CentrePlayerSceneServiceSceneInfoC2SMessageId = 33;
constexpr uint32_t CentrePlayerSceneServiceSceneInfoC2SIndex = 3;
#define CentrePlayerSceneServiceSceneInfoC2SMethod  ::CentrePlayerSceneService_Stub::descriptor()->method(3)


