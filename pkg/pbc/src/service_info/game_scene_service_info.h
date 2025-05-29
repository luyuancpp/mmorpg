#pragma once
#include <cstdint>

#include "proto/logic/server/game_scene.pb.h"


constexpr uint32_t GameSceneServiceTestMessageId = 26;
constexpr uint32_t GameSceneServiceTestIndex = 0;
#define GameSceneServiceTestMethod  ::GameSceneService_Stub::descriptor()->method(0)


