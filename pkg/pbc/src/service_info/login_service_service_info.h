#pragma once
#include <cstdint>

#include "proto/login/login_service.pb.h"


constexpr uint32_t LoginServiceLoginMessageId = 77;
constexpr uint32_t LoginServiceLoginIndex = 0;
#define LoginServiceLoginMethod  ::LoginService_Stub::descriptor()->method(0)

constexpr uint32_t LoginServiceCreatePlayerMessageId = 74;
constexpr uint32_t LoginServiceCreatePlayerIndex = 1;
#define LoginServiceCreatePlayerMethod  ::LoginService_Stub::descriptor()->method(1)

constexpr uint32_t LoginServiceEnterGameMessageId = 48;
constexpr uint32_t LoginServiceEnterGameIndex = 2;
#define LoginServiceEnterGameMethod  ::LoginService_Stub::descriptor()->method(2)

constexpr uint32_t LoginServiceLeaveGameMessageId = 62;
constexpr uint32_t LoginServiceLeaveGameIndex = 3;
#define LoginServiceLeaveGameMethod  ::LoginService_Stub::descriptor()->method(3)

constexpr uint32_t LoginServiceDisconnectMessageId = 83;
constexpr uint32_t LoginServiceDisconnectIndex = 4;
#define LoginServiceDisconnectMethod  ::LoginService_Stub::descriptor()->method(4)


