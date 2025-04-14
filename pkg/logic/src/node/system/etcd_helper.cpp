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

	std::string key = serviceName + "/zone/" + std::to_string(nodeInfo.zone_id()) + "/" + std::to_string(nodeInfo.node_id());
	request.set_key(key);

	auto status = google::protobuf::util::MessageToJsonString(nodeInfo, request.mutable_value());
	if (!status.ok()) {
		LOG_ERROR << "Failed to convert node info to JSON: " << status.message().data();
	}

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

