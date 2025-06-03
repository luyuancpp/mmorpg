#pragma once
#include <cstdint>

#include "proto/db/db_service.pb.h"


constexpr uint32_t AccountDBServiceLoad2RedisMessageId = 68;
constexpr uint32_t AccountDBServiceLoad2RedisIndex = 0;
#define AccountDBServiceLoad2RedisMethod  ::AccountDBService_Stub::descriptor()->method(0)

constexpr uint32_t AccountDBServiceSave2RedisMessageId = 29;
constexpr uint32_t AccountDBServiceSave2RedisIndex = 1;
#define AccountDBServiceSave2RedisMethod  ::AccountDBService_Stub::descriptor()->method(1)



constexpr uint32_t PlayerDBServiceLoad2RedisMessageId = 31;
constexpr uint32_t PlayerDBServiceLoad2RedisIndex = 0;
#define PlayerDBServiceLoad2RedisMethod  ::PlayerDBService_Stub::descriptor()->method(0)

constexpr uint32_t PlayerDBServiceSave2RedisMessageId = 1;
constexpr uint32_t PlayerDBServiceSave2RedisIndex = 1;
#define PlayerDBServiceSave2RedisMethod  ::PlayerDBService_Stub::descriptor()->method(1)


