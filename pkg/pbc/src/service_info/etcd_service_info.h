#pragma once
#include <cstdint>

#include "proto/etcd/etcd.pb.h"

constexpr uint32_t KVLeaseGrantMessageId = 88;
constexpr uint32_t KVLeaseGrantIndex = 0;
#define KVLeaseGrantMethod  ::KV_Stub::descriptor()->method(0)

constexpr uint32_t KVLeaseRevokeMessageId = 87;
constexpr uint32_t KVLeaseRevokeIndex = 1;
#define KVLeaseRevokeMethod  ::KV_Stub::descriptor()->method(1)

constexpr uint32_t KVLeaseKeepAliveMessageId = 83;
constexpr uint32_t KVLeaseKeepAliveIndex = 2;
#define KVLeaseKeepAliveMethod  ::KV_Stub::descriptor()->method(2)

constexpr uint32_t KVLeaseTimeToLiveMessageId = 84;
constexpr uint32_t KVLeaseTimeToLiveIndex = 3;
#define KVLeaseTimeToLiveMethod  ::KV_Stub::descriptor()->method(3)

constexpr uint32_t KVLeaseLeasesMessageId = 85;
constexpr uint32_t KVLeaseLeasesIndex = 4;
#define KVLeaseLeasesMethod  ::KV_Stub::descriptor()->method(4)

