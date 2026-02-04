#pragma once
#include <cstdint>

#include "proto/service/rpc/scene/game_scene.pb.h"


constexpr uint32_t SceneSceneTestMessageId = 18;
constexpr uint32_t SceneSceneTestIndex = 0;
#define SceneSceneTestMethod  ::SceneScene_Stub::descriptor()->method(0)


