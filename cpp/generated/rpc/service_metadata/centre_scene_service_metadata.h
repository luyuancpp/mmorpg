#pragma once
#include <cstdint>

#include "proto/service/cpp/rpc/centre/centre_scene.pb.h"


constexpr uint32_t CentreSceneRegisterSceneMessageId = 44;
constexpr uint32_t CentreSceneRegisterSceneIndex = 0;
#define CentreSceneRegisterSceneMethod  ::CentreScene_Stub::descriptor()->method(0)

constexpr uint32_t CentreSceneUnRegisterSceneMessageId = 22;
constexpr uint32_t CentreSceneUnRegisterSceneIndex = 1;
#define CentreSceneUnRegisterSceneMethod  ::CentreScene_Stub::descriptor()->method(1)


