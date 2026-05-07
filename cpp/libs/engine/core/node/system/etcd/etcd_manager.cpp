#include "etcd_manager.h"
#include "proto/common/base/common.pb.h"
#include "node/system/node/node_util.h"
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

std::string EtcdManager::GetServiceName(uint32_t type)
{
	return eNodeType_Name(type) + ".rpc";
}

std::string EtcdManager::MakeNodeEtcdPrefix(const NodeInfo &info)
{
	return GetServiceName(info.node_type()) +
		   "/zone/" + std::to_string(info.zone_id()) +
		   "/node_type/" + std::to_string(info.node_type()) +
		   "/node_id/";
}

std::string EtcdManager::MakeNodeEtcdKey(const NodeInfo &info)
{
	return MakeNodeEtcdPrefix(info) + std::to_string(info.node_id());
}

std::string EtcdManager::MakeNodeAllocationKey(const NodeInfo &info)
{
	// Zone-independent: the key contains node_type and node_id only. Two
	// zones issuing concurrent PutIfAbsent on the same (node_type, node_id)
	// will see exactly one CAS succeed; the loser has to pick a different
	// node_id. This matches the Go-side allocator layout so mixed-language
	// deployments share a single source of truth for node_id uniqueness.
	return GetServiceName(info.node_type()) +
		   "/allocated/node_type/" + std::to_string(info.node_type()) +
		   "/node_id/" + std::to_string(info.node_id());
}

std::string EtcdManager::MakeNodePortEtcdPrefix(const NodeInfo &nodeInfo)
{
	return "/service/" + nodeInfo.endpoint().ip() + "/port/";
}

std::string EtcdManager::MakeNodePortEtcdKey(const NodeInfo &nodeInfo)
{
	return MakeNodePortEtcdPrefix(nodeInfo) + std::to_string(nodeInfo.endpoint().port());
}

void EtcdManager::RegisterNodeService()
{
	// Two CAS steps under the same lease:
	//   1. Claim the global allocation key `.../allocated/node_type/T/node_id/N`.
	//      This is zone-independent so two zones racing on the same node_id lose
	//      exactly one to version!=0 and must try again.
	//   2. Publish the per-zone NodeInfo under the discovery key for watchers.
	// Both writes share the same lease so they live and die together. On
	// shutdown (Close / lease revoke) both keys disappear, freeing the slot.
	const auto &info = gNode->GetNodeInfo();
	const auto allocKey = MakeNodeAllocationKey(info);
	const auto serviceKey = MakeNodeEtcdKey(info);

	LOG_INFO << "Claiming global node-id allocation: " << allocKey;
	EtcdHelper::PutIfAbsent(allocKey, info.node_uuid(), 0, gNode->GetLeaseId());
	pendingKeys.push_back(allocKey);

	LOG_INFO << "Registering node service to etcd with key: " << serviceKey;
	EtcdHelper::PutIfAbsent(serviceKey, info, gNode->GetLeaseId());
	pendingKeys.push_back(serviceKey);
	LOG_INFO << "Registered node to etcd: " << info.DebugString();
}

void EtcdManager::UpdateNodeInfo()
{
	const auto serviceKey = MakeNodeEtcdKey(gNode->GetNodeInfo());
	EtcdHelper::PutServiceNodeInfo(gNode->GetNodeInfo(), serviceKey, gNode->GetLeaseId());
}

void EtcdManager::RegisterNodePort()
{
	const auto portKey = MakeNodePortEtcdKey(gNode->GetNodeInfo());
	LOG_INFO << "Registering node port to etcd with key: " << portKey;
	EtcdHelper::PutIfAbsent(portKey, "", 0, gNode->GetLeaseId());
	pendingKeys.push_back(portKey);
	LOG_INFO << "Registered node port to etcd: " << gNode->GetNodeInfo().endpoint().port();
}

void EtcdManager::RequestNodeLease()
{
	uint64_t ttlSeconds = tlsNodeConfigManager.GetBaseDeployConfig().node_ttl_seconds();
	LOG_INFO << "[EtcdLease] Requesting lease with TTL: " << ttlSeconds
			 << " seconds. Time: " << muduo::Timestamp::now().toFormattedString();
	LOG_DEBUG << "[EtcdLease] Calling EtcdHelper::GrantLease...";
	EtcdHelper::GrantLease(ttlSeconds);
	LOG_INFO << "[EtcdLease] Lease request completed.";
}

void EtcdManager::StartLeaseKeepAlive()
{
	leaseKeepAliveTimer.RunEvery(tlsNodeConfigManager.GetBaseDeployConfig().keep_alive_interval(), []()
								 {
		etcdserverpb::LeaseKeepAliveRequest req;
		req.set_id(gNode->GetLeaseId());
		SendLeaseLeaseKeepAlive(tlsNodeContextManager.GetRegistry(EtcdNodeService), tlsNodeContextManager.GetGlobalEntity(EtcdNodeService), req);
		LOG_DEBUG << "Keeping node alive, lease_id: " << gNode->GetLeaseId();

		gNode->GetEtcdManager().WriteSnowFlakeGuard(); });
}

std::string EtcdManager::MakeSnowFlakeGuardKey(const NodeInfo &info)
{
	return "snowflake_guard:" + std::to_string(info.zone_id()) + ":" + std::to_string(info.node_type()) + ":" + std::to_string(info.node_id());
}

void EtcdManager::WriteSnowFlakeGuard()
{
	auto &redis = tlsRedis.GetZoneRedis();
	if (!redis || !redis->connected())
	{
		return;
	}

	constexpr uint32_t guardTtl = 600; // 10 minutes — long enough for any restart scenario
	const auto &info = gNode->GetNodeInfo();
	std::string key = MakeSnowFlakeGuardKey(info);
	uint64_t nowSeconds = TimeSystem::NowSecondsUTC();

	redis->command([](hiredis::Hiredis *, redisReply *) {},
				   "SETEX %s %u %llu", key.c_str(), guardTtl, static_cast<unsigned long long>(nowSeconds));
}
