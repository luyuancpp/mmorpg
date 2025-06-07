#pragma once
#include <cstdint>

#include "proto/centre/centre_player_scene.pb.h"


constexpr uint32_t CentrePlayerSceneServiceEnterSceneMessageId = 11;
constexpr uint32_t CentrePlayerSceneServiceEnterSceneIndex = 0;
#define CentrePlayerSceneServiceEnterSceneMethod  ::CentrePlayerSceneService_Stub::descriptor()->method(0)

constexpr uint32_t CentrePlayerSceneServiceLeaveSceneMessageId = 70;
constexpr uint32_t CentrePlayerSceneServiceLeaveSceneIndex = 1;
#define CentrePlayerSceneServiceLeaveSceneMethod  ::CentrePlayerSceneService_Stub::descriptor()->method(1)

constexpr uint32_t CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId = 34;
constexpr uint32_t CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteIndex = 2;
#define CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMethod  ::CentrePlayerSceneService_Stub::descriptor()->method(2)

constexpr uint32_t CentrePlayerSceneServiceSceneInfoC2SMessageId = 63;
constexpr uint32_t CentrePlayerSceneServiceSceneInfoC2SIndex = 3;
#define CentrePlayerSceneServiceSceneInfoC2SMethod  ::CentrePlayerSceneService_Stub::descriptor()->method(3)


