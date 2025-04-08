#pragma once
#include <cstdint>

#include "proto/etcd/etcd.pb.h"

constexpr uint32_t LeaseLeaseGrantMessageId = 86;
constexpr uint32_t LeaseLeaseGrantIndex = 6;
#define LeaseLeaseGrantMethod  ::Lease_Stub::descriptor()->method(6)

constexpr uint32_t LeaseLeaseRevokeMessageId = 81;
constexpr uint32_t LeaseLeaseRevokeIndex = 7;
#define LeaseLeaseRevokeMethod  ::Lease_Stub::descriptor()->method(7)

constexpr uint32_t LeaseLeaseKeepAliveMessageId = 83;
constexpr uint32_t LeaseLeaseKeepAliveIndex = 8;
#define LeaseLeaseKeepAliveMethod  ::Lease_Stub::descriptor()->method(8)

constexpr uint32_t LeaseLeaseTimeToLiveMessageId = 84;
constexpr uint32_t LeaseLeaseTimeToLiveIndex = 9;
#define LeaseLeaseTimeToLiveMethod  ::Lease_Stub::descriptor()->method(9)

constexpr uint32_t LeaseLeaseLeasesMessageId = 87;
constexpr uint32_t LeaseLeaseLeasesIndex = 10;
#define LeaseLeaseLeasesMethod  ::Lease_Stub::descriptor()->method(10)

