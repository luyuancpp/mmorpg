#pragma once
#include <cstdint>

#include "common/deploy_service.pb.h"

constexpr uint32_t DeployServiceGetNodeInfoMessageId = 10;
constexpr uint32_t DeployServiceGetNodeInfoIndex = 0;
#define DeployServiceGetNodeInfoMethod  ::DeployService_Stub::descriptor()->method(0)

