#pragma once
#include <cstdint>

#include "proto/centre/centre_scene.pb.h"


constexpr uint32_t CentreSceneRegisterSceneMessageId = 39;
constexpr uint32_t CentreSceneRegisterSceneIndex = 0;
#define CentreSceneRegisterSceneMethod  ::CentreScene_Stub::descriptor()->method(0)

constexpr uint32_t CentreSceneUnRegisterSceneMessageId = 5;
constexpr uint32_t CentreSceneUnRegisterSceneIndex = 1;
#define CentreSceneUnRegisterSceneMethod  ::CentreScene_Stub::descriptor()->method(1)


