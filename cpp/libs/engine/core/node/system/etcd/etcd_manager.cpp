#include "etcd_manager.h"
#include "proto/common/base/common.pb.h"
#include "proto/common/base/node.pb.h"
#include <muduo/base/Logging.h>
#include "etcd_helper.h"
#include <thread_context/redis_manager.h>
#include "grpc_client/etcd/etcd_grpc_client.h"
#include "thread_context/node_context_manager.h"
#include <node_config_manager.h>
#include <node/system/node/node.h>
#include <time/system/time.h>

void EtcdManager::Shutdown()
{
	leaseKeepAliveTimer.Cancel();
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

void EtcdManager::ForceRegisterNodeService() {
	const auto serviceKey = MakeNodeEtcdKey(gNode->GetNodeInfo());
	LOG_INFO << "Force registering (singleton) node service to etcd: " << serviceKey;
	EtcdHelper::ForcePut(serviceKey, gNode->GetNodeInfo(), gNode->GetLeaseId());
	pendingKeys.push_back(serviceKey);
}

void EtcdManager::RegisterNodePort() {
	const auto portKey = MakeNodePortEtcdKey(gNode->GetNodeInfo());
	LOG_INFO << "Registering node port to etcd with key: " << portKey;
	EtcdHelper::PutIfAbsent(portKey, "", 0, gNode->GetLeaseId());
	pendingKeys.push_back(portKey);
	LOG_INFO << "Registered node port to etcd: " << gNode->GetNodeInfo().endpoint().port();
}

void EtcdManager::RequestNodeLease() {
	uint64_t ttlSeconds = tlsNodeConfigManager.GetBaseDeployConfig().node_ttl_seconds();
	LOG_INFO << "[EtcdLease] Requesting lease with TTL: " << ttlSeconds
		<< " seconds. Time: " << muduo::Timestamp::now().toFormattedString();
	LOG_DEBUG << "[EtcdLease] Calling EtcdHelper::GrantLease...";
	EtcdHelper::GrantLease(ttlSeconds);
	LOG_INFO << "[EtcdLease] Lease request completed.";
}

void EtcdManager::StartLeaseKeepAlive() {
	leaseKeepAliveTimer.RunEvery(tlsNodeConfigManager.GetBaseDeployConfig().keep_alive_interval(), []() {
		etcdserverpb::LeaseKeepAliveRequest req;
		req.set_id(gNode->GetLeaseId());
		SendLeaseLeaseKeepAlive(tlsNodeContextManager.GetRegistry(EtcdNodeService), tlsNodeContextManager.GetGlobalEntity(EtcdNodeService), req);
		LOG_DEBUG << "Keeping node alive, lease_id: " << gNode->GetLeaseId();

		gNode->GetEtcdManager().WriteSnowFlakeGuard();
		});
}

std::string EtcdManager::MakeSnowFlakeGuardKey(const NodeInfo& info) {
	return "snowflake_guard:" + std::to_string(info.zone_id())
		+ ":" + std::to_string(info.node_type())
		+ ":" + std::to_string(info.node_id());
}

void EtcdManager::WriteSnowFlakeGuard() {
	auto& redis = tlsReids.GetZoneRedis();
	if (!redis || !redis->connected()) {
		return;
	}

	constexpr uint32_t guardTtl = 600; // 10 minutes — long enough for any restart scenario
	const auto& info = gNode->GetNodeInfo();
	std::string key = MakeSnowFlakeGuardKey(info);
	uint64_t nowSeconds = TimeSystem::NowSecondsUTC();

	redis->command([](hiredis::Hiredis*, redisReply*) {},
		"SETEX %s %u %llu", key.c_str(), guardTtl, static_cast<unsigned long long>(nowSeconds));
}

