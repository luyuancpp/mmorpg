#pragma once
#include <cstdint>

#include "proto/scene/game_scene.pb.h"


constexpr uint32_t GameSceneServiceTestMessageId = 47;
constexpr uint32_t GameSceneServiceTestIndex = 0;
#define GameSceneServiceTestMethod  ::GameSceneService_Stub::descriptor()->method(0)


