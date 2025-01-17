#pragma once
#include <cstdint>

#include "proto/common/deploy_service.pb.h"

constexpr uint32_t DeployServiceGetNodeInfoMessageId = 10;
constexpr uint32_t DeployServiceGetNodeInfoIndex = 0;
#define DeployServiceGetNodeInfoMethod  ::DeployService_Stub::descriptor()->method(0)

constexpr uint32_t DeployServiceGetIDMessageId = 72;
constexpr uint32_t DeployServiceGetIDIndex = 1;
#define DeployServiceGetIDMethod  ::DeployService_Stub::descriptor()->method(1)

constexpr uint32_t DeployServiceReleaseIDMessageId = 73;
constexpr uint32_t DeployServiceReleaseIDIndex = 2;
#define DeployServiceReleaseIDMethod  ::DeployService_Stub::descriptor()->method(2)

constexpr uint32_t DeployServiceRenewLeaseMessageId = 74;
constexpr uint32_t DeployServiceRenewLeaseIndex = 3;
#define DeployServiceRenewLeaseMethod  ::DeployService_Stub::descriptor()->method(3)

