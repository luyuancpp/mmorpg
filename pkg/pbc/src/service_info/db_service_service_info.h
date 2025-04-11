#pragma once
#include <cstdint>

#include "proto/common/db_service.pb.h"

constexpr uint32_t PlayerDBServiceLoad2RedisMessageId = 88;
constexpr uint32_t PlayerDBServiceLoad2RedisIndex = 0;
#define PlayerDBServiceLoad2RedisMethod  ::PlayerDBService_Stub::descriptor()->method(0)

constexpr uint32_t PlayerDBServiceSave2RedisMessageId = 81;
constexpr uint32_t PlayerDBServiceSave2RedisIndex = 1;
#define PlayerDBServiceSave2RedisMethod  ::PlayerDBService_Stub::descriptor()->method(1)

