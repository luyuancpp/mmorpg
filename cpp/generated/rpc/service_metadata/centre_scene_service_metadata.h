#pragma once
#include <cstdint>

#include "proto/service/rpc/centre/centre_scene.pb.h"


constexpr uint32_t CentreSceneRegisterSceneMessageId = 3;
constexpr uint32_t CentreSceneRegisterSceneIndex = 0;
#define CentreSceneRegisterSceneMethod  ::CentreScene_Stub::descriptor()->method(0)

constexpr uint32_t CentreSceneUnRegisterSceneMessageId = 20;
constexpr uint32_t CentreSceneUnRegisterSceneIndex = 1;
#define CentreSceneUnRegisterSceneMethod  ::CentreScene_Stub::descriptor()->method(1)


