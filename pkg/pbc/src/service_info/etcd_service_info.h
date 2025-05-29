#pragma once
#include <cstdint>

#include "proto/etcd/etcd.pb.h"


constexpr uint32_t KVRangeMessageId = 77;
constexpr uint32_t KVRangeIndex = 0;
#define KVRangeMethod  ::KV_Stub::descriptor()->method(0)

constexpr uint32_t KVPutMessageId = 76;
constexpr uint32_t KVPutIndex = 1;
#define KVPutMethod  ::KV_Stub::descriptor()->method(1)

constexpr uint32_t KVDeleteRangeMessageId = 78;
constexpr uint32_t KVDeleteRangeIndex = 2;
#define KVDeleteRangeMethod  ::KV_Stub::descriptor()->method(2)

constexpr uint32_t KVTxnMessageId = 79;
constexpr uint32_t KVTxnIndex = 3;
#define KVTxnMethod  ::KV_Stub::descriptor()->method(3)

constexpr uint32_t KVCompactMessageId = 80;
constexpr uint32_t KVCompactIndex = 4;
#define KVCompactMethod  ::KV_Stub::descriptor()->method(4)



constexpr uint32_t WatchWatchMessageId = 82;
constexpr uint32_t WatchWatchIndex = 0;
#define WatchWatchMethod  ::Watch_Stub::descriptor()->method(0)



constexpr uint32_t LeaseLeaseGrantMessageId = 87;
constexpr uint32_t LeaseLeaseGrantIndex = 0;
#define LeaseLeaseGrantMethod  ::Lease_Stub::descriptor()->method(0)

constexpr uint32_t LeaseLeaseRevokeMessageId = 83;
constexpr uint32_t LeaseLeaseRevokeIndex = 1;
#define LeaseLeaseRevokeMethod  ::Lease_Stub::descriptor()->method(1)

constexpr uint32_t LeaseLeaseKeepAliveMessageId = 84;
constexpr uint32_t LeaseLeaseKeepAliveIndex = 2;
#define LeaseLeaseKeepAliveMethod  ::Lease_Stub::descriptor()->method(2)

constexpr uint32_t LeaseLeaseTimeToLiveMessageId = 86;
constexpr uint32_t LeaseLeaseTimeToLiveIndex = 3;
#define LeaseLeaseTimeToLiveMethod  ::Lease_Stub::descriptor()->method(3)

constexpr uint32_t LeaseLeaseLeasesMessageId = 85;
constexpr uint32_t LeaseLeaseLeasesIndex = 4;
#define LeaseLeaseLeasesMethod  ::Lease_Stub::descriptor()->method(4)


