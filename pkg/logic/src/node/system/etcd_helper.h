#pragma once

#include <string>
#include "proto/common/common.pb.h"

namespace EtcdHelper {
	void PutServiceNodeInfo(const NodeInfo& nodeInfo, const std::string& key);
	void RangeQuery(const std::string& prefix);
	void StartWatchingPrefix(const std::string& prefix);
	void StopAllWatching();  // 可扩展的占位函数
	void GrantLease(uint32_t ttlSeconds);
	void PutIfAbsent(const std::string& key, const std::string& newValue, int64_t currentVersion, int64_t lease);
	void PutIfAbsent(const std::string& key, const NodeInfo& nodeInfo);
	void RevokeLeaseAndCleanup(int64_t leaseId);
}
