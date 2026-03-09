#pragma once
#include <cstdint>

#include "proto/scene_manager/scene_manager_service.pb.h"


constexpr uint32_t SceneManagerCreateSceneMessageId = 44;
constexpr uint32_t SceneManagerCreateSceneIndex = 0;
#define SceneManagerCreateSceneMethod  ::SceneManager_Stub::descriptor()->method(0)

constexpr uint32_t SceneManagerDestroySceneMessageId = 16;
constexpr uint32_t SceneManagerDestroySceneIndex = 1;
#define SceneManagerDestroySceneMethod  ::SceneManager_Stub::descriptor()->method(1)

constexpr uint32_t SceneManagerEnterSceneByCentreMessageId = 46;
constexpr uint32_t SceneManagerEnterSceneByCentreIndex = 2;
#define SceneManagerEnterSceneByCentreMethod  ::SceneManager_Stub::descriptor()->method(2)

constexpr uint32_t SceneManagerLeaveSceneByCentreMessageId = 85;
constexpr uint32_t SceneManagerLeaveSceneByCentreIndex = 3;
#define SceneManagerLeaveSceneByCentreMethod  ::SceneManager_Stub::descriptor()->method(3)

constexpr uint32_t SceneManagerGateConnectMessageId = 86;
constexpr uint32_t SceneManagerGateConnectIndex = 4;
#define SceneManagerGateConnectMethod  ::SceneManager_Stub::descriptor()->method(4)


