#pragma once
#include <cstdint>

#include "proto/service/go/grpc/db/db.pb.h"


constexpr uint32_t dbTestMessageId = 3;
constexpr uint32_t dbTestIndex = 0;
#define dbTestMethod  ::db_Stub::descriptor()->method(0)


