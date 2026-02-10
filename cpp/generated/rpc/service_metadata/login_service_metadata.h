#pragma once
#include <cstdint>

#include "proto/login/login.pb.h"


constexpr uint32_t ClientPlayerLoginLoginMessageId = 48;
constexpr uint32_t ClientPlayerLoginLoginIndex = 0;
#define ClientPlayerLoginLoginMethod  ::ClientPlayerLogin_Stub::descriptor()->method(0)

constexpr uint32_t ClientPlayerLoginCreatePlayerMessageId = 14;
constexpr uint32_t ClientPlayerLoginCreatePlayerIndex = 1;
#define ClientPlayerLoginCreatePlayerMethod  ::ClientPlayerLogin_Stub::descriptor()->method(1)

constexpr uint32_t ClientPlayerLoginEnterGameMessageId = 26;
constexpr uint32_t ClientPlayerLoginEnterGameIndex = 2;
#define ClientPlayerLoginEnterGameMethod  ::ClientPlayerLogin_Stub::descriptor()->method(2)

constexpr uint32_t ClientPlayerLoginLeaveGameMessageId = 17;
constexpr uint32_t ClientPlayerLoginLeaveGameIndex = 3;
#define ClientPlayerLoginLeaveGameMethod  ::ClientPlayerLogin_Stub::descriptor()->method(3)

constexpr uint32_t ClientPlayerLoginDisconnectMessageId = 58;
constexpr uint32_t ClientPlayerLoginDisconnectIndex = 4;
#define ClientPlayerLoginDisconnectMethod  ::ClientPlayerLogin_Stub::descriptor()->method(4)


