#pragma once
#include <cstdint>

#include "proto/common/db_service.pb.h"

constexpr uint32_t PlayerDBServiceLoad2RedisMessageId = 82;
constexpr uint32_t PlayerDBServiceLoad2RedisIndex = 2;
#define PlayerDBServiceLoad2RedisMethod  ::PlayerDBService_Stub::descriptor()->method(2)

constexpr uint32_t PlayerDBServiceSave2RedisMessageId = 88;
constexpr uint32_t PlayerDBServiceSave2RedisIndex = 3;
#define PlayerDBServiceSave2RedisMethod  ::PlayerDBService_Stub::descriptor()->method(3)

