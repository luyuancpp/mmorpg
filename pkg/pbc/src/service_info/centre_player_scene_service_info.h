#pragma once
#include <cstdint>

#include "proto/centre/centre_player_scene.pb.h"


constexpr uint32_t CentrePlayerSceneEnterSceneMessageId = 11;
constexpr uint32_t CentrePlayerSceneEnterSceneIndex = 0;
#define CentrePlayerSceneEnterSceneMethod  ::CentrePlayerScene_Stub::descriptor()->method(0)

constexpr uint32_t CentrePlayerSceneLeaveSceneMessageId = 70;
constexpr uint32_t CentrePlayerSceneLeaveSceneIndex = 1;
#define CentrePlayerSceneLeaveSceneMethod  ::CentrePlayerScene_Stub::descriptor()->method(1)

constexpr uint32_t CentrePlayerSceneLeaveSceneAsyncSavePlayerCompleteMessageId = 34;
constexpr uint32_t CentrePlayerSceneLeaveSceneAsyncSavePlayerCompleteIndex = 2;
#define CentrePlayerSceneLeaveSceneAsyncSavePlayerCompleteMethod  ::CentrePlayerScene_Stub::descriptor()->method(2)

constexpr uint32_t CentrePlayerSceneSceneInfoC2SMessageId = 63;
constexpr uint32_t CentrePlayerSceneSceneInfoC2SIndex = 3;
#define CentrePlayerSceneSceneInfoC2SMethod  ::CentrePlayerScene_Stub::descriptor()->method(3)


