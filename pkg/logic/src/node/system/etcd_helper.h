#pragma once

#include <string>
#include "proto/etcd/etcd.pb.h"
#include "logic/constants/node.pb.h"
#include "proto/common/common.pb.h"

namespace EtcdHelper {
	void PutServiceNodeInfo(const NodeInfo& nodeInfo, const std::string& key);
	void RangeQuery(const std::string& prefix);
	void StartWatchingPrefix(const std::string& prefix);
	void StopAllWatching();  // 可扩展的占位函数
	void GrantLease(uint32_t ttlSeconds);
}
