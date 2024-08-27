#pragma once
#include <cstdint>

#include "logic/server/game_scene.pb.h"

constexpr uint32_t GameSceneServiceTestMessageId = 48;
constexpr uint32_t GameSceneServiceTestIndex = 0;
#define GameSceneServiceTestMethod  ::GameSceneService_Stub::descriptor()->method(0)

