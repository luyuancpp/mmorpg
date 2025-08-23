#pragma once
#include <cstdint>

#include "proto/db/db_service.pb.h"


constexpr uint32_t dbserviceTestMessageId = 23;
constexpr uint32_t dbserviceTestIndex = 0;
#define dbserviceTestMethod  ::dbservice_Stub::descriptor()->method(0)


