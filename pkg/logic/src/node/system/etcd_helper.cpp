#include "etcd_helper.h"
#include <google/protobuf/util/json_util.h>
#include "thread_local/storage_common_logic.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "network/network_constants.h"
#include "log/system/console_log_system.h"
#include <muduo/base/Logging.h>
#include <thread_local/storage.h>

void EtcdHelper::PutServiceNodeInfo(const NodeInfo& nodeInfo, const std::string& serviceName) {
    etcdserverpb::PutRequest request;

    // 构造 etcd 中的键名，结构如下：
    // {serviceName}/zone/{zone_id}/node_type/{node_type}/node_id/{node_id}
    std::string key = serviceName +
        "/zone/" + std::to_string(nodeInfo.zone_id()) +
        "/node_type/" + std::to_string(nodeInfo.node_type()) +
        "/node_id/" + std::to_string(nodeInfo.node_id());
    request.set_key(key);

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

