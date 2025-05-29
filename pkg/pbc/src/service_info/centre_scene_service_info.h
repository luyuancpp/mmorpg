#pragma once
#include <cstdint>

#include "proto/centre/centre_scene.pb.h"


constexpr uint32_t CentreSceneServiceRegisterSceneMessageId = 73;
constexpr uint32_t CentreSceneServiceRegisterSceneIndex = 0;
#define CentreSceneServiceRegisterSceneMethod  ::CentreSceneService_Stub::descriptor()->method(0)

constexpr uint32_t CentreSceneServiceUnRegisterSceneMessageId = 52;
constexpr uint32_t CentreSceneServiceUnRegisterSceneIndex = 1;
#define CentreSceneServiceUnRegisterSceneMethod  ::CentreSceneService_Stub::descriptor()->method(1)


