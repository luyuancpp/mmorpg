#include "etcd_helper.h"
#include <google/protobuf/util/json_util.h>
#include "thread_local/storage_common_logic.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "network/network_constants.h"
#include "log/system/console_log_system.h"
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

    SendetcdserverpbKVPut(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);

}

void EtcdHelper::RangeQuery(const std::string& prefix) {
	etcdserverpb::RangeRequest request;
	request.set_key(prefix);

	std::string range_end = prefix;
	range_end.back() += 1; // last char + 1
	request.set_range_end(range_end);

	SendetcdserverpbKVRange(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);
}

void EtcdHelper::StartWatchingPrefix(const std::string& prefix) {
	etcdserverpb::WatchRequest request;
	auto& createReq = *request.mutable_create_request();

	createReq.set_key(prefix);

	std::string range_end = prefix;
	range_end.back() += 1;
	createReq.set_range_end(range_end);

	SendetcdserverpbWatchWatch(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);
}

void EtcdHelper::StopAllWatching() {
	// TODO: Add cancel_request implementation if needed
}

void EtcdHelper::GrantLease(uint32_t ttlSeconds) {
	// 创建 LeaseGrantRequest 请求对象
	etcdserverpb::LeaseGrantRequest leaseReq;
	leaseReq.set_ttl(ttlSeconds);  // 设置 TTL（生存时间）

	SendetcdserverpbLeaseLeaseGrant(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), leaseReq);
}

void EtcdHelper::CompareAndPutWithRetry(const std::string& key, const std::string& newValue, int64_t currentVersion) {
	// 创建事务请求
	etcdserverpb::TxnRequest txn;

	// 添加版本比较条件
	etcdserverpb::Compare* compare = txn.add_compare();
	compare->set_key(key);
	compare->set_result(etcdserverpb::Compare::EQUAL);  // 比较结果：等于
	compare->set_target(etcdserverpb::Compare::MOD);     // 比较键的版本
	compare->set_version(currentVersion);    // 设置当前版本号

	// 添加成功时的操作（如果版本匹配）
	auto& sucessOp = *txn.add_success();
	sucessOp.mutable_request_put()->set_key(key);
	sucessOp.mutable_request_put()->set_value(newValue);  // 如果版本匹配，更新值

	SendetcdserverpbKVTxn(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), txn);
}