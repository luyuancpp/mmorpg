#include "etcd_manager.h"
#include "proto/common/common.pb.h"
#include "proto/common/node.pb.h"
#include <muduo/base/Logging.h>
#include "etcd_helper.h"
#include "node.h"
#include <thread_local/storage_common_logic.h>
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "thread_local/thread_local_node_context.h"

void EtcdManager::Shutdown()
{
	renewLeaseTimer.Cancel();
}

std::string EtcdManager::GetServiceName(uint32_t type) {
	return eNodeType_Name(type) + ".rpc";
}

std::string EtcdManager::MakeNodeEtcdPrefix(const NodeInfo& info)
{
	return GetServiceName(info.node_type()) +
		"/zone/" + std::to_string(info.zone_id()) +
		"/node_type/" + std::to_string(info.node_type()) +
		"/node_id/";
}

std::string EtcdManager::MakeNodeEtcdKey(const NodeInfo& info) {
	return MakeNodeEtcdPrefix(info) + std::to_string(info.node_id());
}

std::string EtcdManager::MakeNodePortEtcdPrefix(const NodeInfo& nodeInfo)
{
	return  "/service/" + nodeInfo.endpoint().ip() + "/port/";
}

std::string EtcdManager::MakeNodePortEtcdKey(const NodeInfo& nodeInfo)
{
	return MakeNodePortEtcdPrefix(nodeInfo) + std::to_string(nodeInfo.endpoint().port());
}

void EtcdManager::RegisterNodeService() {
	const auto serviceKey = MakeNodeEtcdKey(gNode->GetNodeInfo());
	LOG_INFO << "Registering node service to etcd with key: " << serviceKey;
	EtcdHelper::PutIfAbsent(serviceKey, gNode->GetNodeInfo(), gNode->GetLeaseId());
	pendingKeys.push_back(serviceKey);
	LOG_INFO << "Registered node to etcd: " << gNode->GetNodeInfo().DebugString();
}

void EtcdManager::RegisterNodePort() {
	const auto portKey = MakeNodePortEtcdKey(gNode->GetNodeInfo());
	LOG_INFO << "Registering node port to etcd with key: " << portKey;
	EtcdHelper::PutIfAbsent(portKey, "", 0, gNode->GetLeaseId());
	pendingKeys.push_back(portKey);
	LOG_INFO << "Registered node port to etcd: " << gNode->GetNodeInfo().endpoint().port();
}

void EtcdManager::RequestEtcdLease() {
	uint64_t ttlSeconds = tlsCommonLogic.GetBaseDeployConfig().node_ttl_seconds();
	LOG_INFO << "[EtcdLease] Requesting lease with TTL: " << ttlSeconds
		<< " seconds. Time: " << muduo::Timestamp::now().toFormattedString();
	LOG_DEBUG << "[EtcdLease] Calling EtcdHelper::GrantLease...";
	EtcdHelper::GrantLease(ttlSeconds);
	LOG_INFO << "[EtcdLease] Lease request completed.";
}

void EtcdManager::KeepNodeAlive() {
	renewLeaseTimer.RunEvery(tlsCommonLogic.GetBaseDeployConfig().keep_alive_interval(), []() {
		etcdserverpb::LeaseKeepAliveRequest req;
		req.set_id(gNode->GetLeaseId());
		SendLeaseLeaseKeepAlive(ThreadLocalNodeContext::Instance().GetRegistry(EtcdNodeService), ThreadLocalNodeContext::Instance().GetGlobalEntity(EtcdNodeService), req);
		LOG_DEBUG << "Keeping node alive, lease_id: " << gNode->GetLeaseId();
		});
}
