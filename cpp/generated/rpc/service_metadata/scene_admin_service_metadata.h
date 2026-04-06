#pragma once
#include <cstdint>

#include "proto/scene/scene_admin.pb.h"

constexpr uint32_t SceneSceneTestMessageId = 18;
constexpr uint32_t SceneSceneTestIndex = 0;
#define SceneSceneTestMethod  ::SceneScene_Stub::descriptor()->method(0)
