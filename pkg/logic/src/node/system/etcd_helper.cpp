#include "etcd_helper.h"
#include <google/protobuf/util/json_util.h>
#include "thread_local/storage_common_logic.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include <muduo/base/Logging.h>
#include <thread_local/storage.h>

void EtcdHelper::PutServiceNodeInfo(const NodeInfo& nodeInfo, const std::string& key) {
    etcdserverpb::PutRequest request;

    request.set_key(key);
	request.set_prev_kv(true);

    // 将 NodeInfo 序列化为 JSON 字符串，设置为 value
    std::string jsonValue;
    auto status = google::protobuf::util::MessageToJsonString(nodeInfo, &jsonValue);
    if (!status.ok()) {
        LOG_ERROR << "[EtcdHelper::PutServiceNodeInfo] Failed to serialize NodeInfo to JSON. "
            << "Error: " << status.message().data()
            << ", key: " << key;
        return;
    }
    request.set_value(jsonValue);

    SendKVPut(tls.GetNodeRegistry(EtcdNodeService), tls.GetNodeGlobalEntity(EtcdNodeService), request);

}

void EtcdHelper::RangeQuery(const std::string& prefix) {
	etcdserverpb::RangeRequest request;
	request.set_key(prefix);

	std::string range_end = prefix;
	range_end.back() += 1; // last char + 1
	request.set_range_end(range_end);

	SendKVRange(tls.GetNodeRegistry(EtcdNodeService), tls.GetNodeGlobalEntity(EtcdNodeService), request);
}

void EtcdHelper::StartWatchingPrefix(const std::string& prefix, int64_t revision) {
	etcdserverpb::WatchRequest request;
	auto& createReq = *request.mutable_create_request();

	createReq.set_prev_kv(true);
	createReq.set_key(prefix);

	std::string range_end = prefix;
	range_end.back() += 1;
	createReq.set_range_end(range_end);

	if (revision > 0) {
		createReq.set_start_revision(revision);
	}

	SendWatchWatch(tls.GetNodeRegistry(EtcdNodeService), tls.GetNodeGlobalEntity(EtcdNodeService), request);
}


void EtcdHelper::StopAllWatching() {
	// TODO: Add cancel_request implementation if needed
}

void EtcdHelper::GrantLease(uint32_t ttlSeconds) {
	// 创建 LeaseGrantRequest 请求对象
	etcdserverpb::LeaseGrantRequest leaseReq;
	leaseReq.set_ttl(ttlSeconds);  // 设置 TTL（生存时间）

	SendLeaseLeaseGrant(tls.GetNodeRegistry(EtcdNodeService), tls.GetNodeGlobalEntity(EtcdNodeService), leaseReq);
}

void EtcdHelper::PutIfAbsent(const std::string& key, const std::string& newValue, int64_t currentVersion, int64_t lease) {
	etcdserverpb::TxnRequest txn;

	// Compare：version == 0 → key 不存在
	auto* compare = txn.add_compare();
	compare->set_key(key);
	compare->set_target(etcdserverpb::Compare::VERSION);
	compare->set_result(etcdserverpb::Compare::EQUAL);
	compare->set_version(currentVersion);

	// Success：put(key, value)
	auto* successOp = txn.add_success()->mutable_request_put();
	successOp->set_key(key);
	successOp->set_value(newValue);
	successOp->set_lease(lease);

	SendKVTxn(tls.GetNodeRegistry(EtcdNodeService), tls.GetNodeGlobalEntity(EtcdNodeService), txn);
}

void EtcdHelper::PutIfAbsent(const std::string& key, const NodeInfo& nodeInfo, int64_t lease)
{
	std::string jsonValue;
	auto status = google::protobuf::util::MessageToJsonString(nodeInfo, &jsonValue);
	if (!status.ok()) {
		LOG_ERROR << " Failed to serialize NodeInfo to JSON. "
			<< "Error: " << status.message().data();
		return;
	}

	PutIfAbsent(key, jsonValue, 0, lease);
}

void EtcdHelper::RevokeLeaseAndCleanup(int64_t leaseId)
{
	etcdserverpb::LeaseRevokeRequest request;
	request.set_id(leaseId);

	SendLeaseLeaseRevoke(tls.GetNodeRegistry(EtcdNodeService), tls.GetNodeGlobalEntity(EtcdNodeService), request);
}

void EtcdHelper::DeleteRange(const std::string& key, bool isPrefix) {
	etcdserverpb::DeleteRangeRequest request;
	request.set_key(key);

	if (isPrefix) {
		std::string range_end = key;
		range_end.back() += 1;  // prefix 范围删除
		request.set_range_end(range_end);
	}

	SendKVDeleteRange(tls.GetNodeRegistry(EtcdNodeService), tls.GetNodeGlobalEntity(EtcdNodeService), request);
}
