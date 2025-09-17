#pragma once
#include <cstdint>

#include "proto/service/go/grpc/login/login.pb.h"


constexpr uint32_t ClientPlayerLoginLoginMessageId = 76;
constexpr uint32_t ClientPlayerLoginLoginIndex = 0;
#define ClientPlayerLoginLoginMethod  ::ClientPlayerLogin_Stub::descriptor()->method(0)

constexpr uint32_t ClientPlayerLoginCreatePlayerMessageId = 68;
constexpr uint32_t ClientPlayerLoginCreatePlayerIndex = 1;
#define ClientPlayerLoginCreatePlayerMethod  ::ClientPlayerLogin_Stub::descriptor()->method(1)

constexpr uint32_t ClientPlayerLoginEnterGameMessageId = 62;
constexpr uint32_t ClientPlayerLoginEnterGameIndex = 2;
#define ClientPlayerLoginEnterGameMethod  ::ClientPlayerLogin_Stub::descriptor()->method(2)

constexpr uint32_t ClientPlayerLoginLeaveGameMessageId = 53;
constexpr uint32_t ClientPlayerLoginLeaveGameIndex = 3;
#define ClientPlayerLoginLeaveGameMethod  ::ClientPlayerLogin_Stub::descriptor()->method(3)

constexpr uint32_t ClientPlayerLoginDisconnectMessageId = 69;
constexpr uint32_t ClientPlayerLoginDisconnectIndex = 4;
#define ClientPlayerLoginDisconnectMethod  ::ClientPlayerLogin_Stub::descriptor()->method(4)


