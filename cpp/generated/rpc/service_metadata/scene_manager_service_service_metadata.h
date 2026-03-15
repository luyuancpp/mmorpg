#pragma once
#include <cstdint>

#include "proto/scene_manager/scene_manager_service.pb.h"

constexpr uint32_t SceneManagerCreateSceneMessageId = 44;
constexpr uint32_t SceneManagerCreateSceneIndex = 0;
#define SceneManagerCreateSceneMethod  ::SceneManager_Stub::descriptor()->method(0)

constexpr uint32_t SceneManagerDestroySceneMessageId = 16;
constexpr uint32_t SceneManagerDestroySceneIndex = 1;
#define SceneManagerDestroySceneMethod  ::SceneManager_Stub::descriptor()->method(1)

constexpr uint32_t SceneManagerEnterSceneMessageId = 46;
constexpr uint32_t SceneManagerEnterSceneIndex = 2;
#define SceneManagerEnterSceneMethod  ::SceneManager_Stub::descriptor()->method(2)

constexpr uint32_t SceneManagerLeaveSceneMessageId = 85;
constexpr uint32_t SceneManagerLeaveSceneIndex = 3;
#define SceneManagerLeaveSceneMethod  ::SceneManager_Stub::descriptor()->method(3)
