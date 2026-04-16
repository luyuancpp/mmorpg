#include "etcd_service.h"
#include "etcd_helper.h"
#include "etcd_manager.h"
#include <google/protobuf/util/json_util.h>
#include <boost/algorithm/string.hpp>
#include "node/system/node/node.h"
#include "node/system/node/node_allocator.h"
#include "grpc_client/grpc_init_client.h"
#include "grpc_client/etcd/etcd_grpc_client.h"
#include "node/system/grpc_channel_cache.h"
#include "thread_context/redis_manager.h"
#include "thread_context/node_context_manager.h"
#include <node_config_manager.h>
#include <thread_context/snow_flake_manager.h>
#include <time/system/time.h>

void EtcdService::Init() {
	InitHandlers();

	const std::string& etcdAddr = *tlsNodeConfigManager.GetBaseDeployConfig().etcd_hosts().begin();
	auto& grpcChannelCache = gNode->GetGrpcChannelCache();
	auto channel = grpcChannelCache.GetOrCreateChannel(etcdAddr);
	LOG_INFO << "gRPC client config: ResourceQuota max threads=" << grpcChannelCache.ConfiguredMaxThreads()
		<< ", backup poll interval ms=" << grpcChannelCache.ConfiguredBackupPollIntervalMs();

	InitGrpcNode(channel, tlsNodeContextManager.GetRegistry(EtcdNodeService), tlsNodeContextManager.GetGlobalEntity(EtcdNodeService));

	grpcHandlerTimer.RunEvery(0.005, [] {
		for (auto& registry : tlsNodeContextManager.GetAllRegistries()) {
			HandleCompletedQueueMessage(registry);
		}
		});

	LOG_INFO << "EtcdService initialized with etcd: " << etcdAddr;
}

void EtcdService::InitHandlers() {
	InitKVHandlers();
	InitWatchHandlers();
	InitLeaseHandlers();
	InitTxnHandlers();
}

void EtcdService::InitKVHandlers() {
	etcdserverpb::AsyncKVRangeHandler = [this](const ClientContext& ctx, const etcdserverpb::RangeResponse& reply) {
		int64_t nextRevision = reply.header().revision() + 1;
		std::unordered_map<std::string, bool> prefixSeen;

		for (const auto& prefix : tlsNodeConfigManager.GetBaseDeployConfig().service_discovery_prefixes()) {
			prefixSeen[prefix] = false;
		}

		for (const auto& kv : reply.kvs()) {
			HandlePutEvent(kv.key(), kv.value());
			for (auto& [prefix, seen] : prefixSeen) {
				if (kv.key().rfind(prefix, 0) == 0) {
					seen = true;
				}
			}
		}

		for (const auto& [prefix, _] : prefixSeen) {
			revision[prefix] = nextRevision;
		}

		if (!hasSentRange) {
			StartWatchingPrefixes();
			hasSentRange = true;
		}
		};

	etcdserverpb::AsyncKVPutHandler = [this](const ClientContext& context, const etcdserverpb::PutResponse& reply) {
		LOG_INFO << "Put response: " << reply.DebugString();
		};

	etcdserverpb::AsyncKVDeleteRangeHandler = [](const ClientContext& context, const etcdserverpb::DeleteRangeResponse& reply) {};

	auto emptyHandler = [](const ClientContext& context, const ::google::protobuf::Message& reply) {};
	if (!etcdserverpb::AsyncKVCompactHandler) {
		etcdserverpb::AsyncKVCompactHandler = emptyHandler;
	}
}

void EtcdService::InitWatchHandlers() {
	etcdserverpb::AsyncWatchWatchHandler = [this](const ClientContext& ctx, const etcdserverpb::WatchResponse& response) {
		OnWatchResponse(response);
		};
}

void EtcdService::InitLeaseHandlers() {
	etcdserverpb::AsyncLeaseLeaseGrantHandler = [this](const ClientContext& context, const etcdserverpb::LeaseGrantResponse& reply) {
		OnLeaseGranted(reply);
		};

	etcdserverpb::AsyncLeaseLeaseKeepAliveHandler = [this](const ClientContext& context, const etcdserverpb::LeaseKeepAliveResponse& reply) {
		OnKeepAliveResponse(reply);
		};
}

void EtcdService::InitTxnHandlers() {
	etcdserverpb::AsyncKVTxnHandler = [this](const ClientContext& context, const etcdserverpb::TxnResponse& reply) {
		LOG_INFO << "Txn response: " << reply.DebugString();

		auto& pendingKeys = gNode->GetEtcdManager().GetPendingKeys();
		if (pendingKeys.empty()) {
			LOG_WARN << "Ignore txn response because pending key queue is empty.";
			return;
		}

		std::string key = std::move(pendingKeys.front());
		pendingKeys.pop_front();

		if (reply.succeeded()) {
			OnTxnSucceeded(key);
		}
		else {
			OnTxnFailed(key);
		}

		};
}

const char* EtcdService::RegistrationModeName(RegistrationMode mode) const {
	switch (mode) {
	case RegistrationMode::kInitialBoot:
		return "InitialBoot";
	case RegistrationMode::kReRegisterExisting:
		return "ReRegisterExisting";
	default:
		return "Unknown";
	}
}

void EtcdService::SetRegistrationMode(RegistrationMode mode, const char* reason) {
	if (registrationMode_ == mode) {
		return;
	}

	LOG_INFO << "Registration mode: " << RegistrationModeName(registrationMode_)
		<< " -> " << RegistrationModeName(mode)
		<< ", reason=" << reason;
	registrationMode_ = mode;
}

bool EtcdService::IsNodePortKey(const std::string& key) const {
	return boost::algorithm::starts_with(key, gNode->GetEtcdManager().MakeNodePortEtcdPrefix(gNode->GetNodeInfo()));
}

bool EtcdService::IsNodeIdKey(const std::string& key) const {
	return boost::algorithm::starts_with(key, gNode->GetEtcdManager().MakeNodeEtcdPrefix(gNode->GetNodeInfo()));
}

void EtcdService::OnTxnSucceeded(const std::string& key) {
	if (IsNodePortKey(key)) {
		if (registrationMode_ == RegistrationMode::kReRegisterExisting) {
			// Re-register path keeps the same node_id and only refreshes lease-bound etcd records.
			gNode->GetEtcdManager().RegisterNodeService();
			return;
		}

		NodeAllocator::AcquireNode();
		return;
	}

	if (!IsNodeIdKey(key)) {
		LOG_WARN << "Unexpected txn success key: " << key;
		return;
	}

	if (registrationMode_ == RegistrationMode::kReRegisterExisting) {
		SetRegistrationMode(RegistrationMode::kInitialBoot, "re-registration succeeded");
		LOG_INFO << "Node re-registration successful, node_id=" << gNode->GetNodeInfo().node_id();
		return;
	}

	ActivateSnowFlakeAfterGuard();
}

void EtcdService::OnTxnFailed(const std::string& key) {
	if (registrationMode_ == RegistrationMode::kReRegisterExisting) {
		SetRegistrationMode(RegistrationMode::kInitialBoot, "re-registration failed");
		gNode->OnNodeIdConflictShutdown(NodeIdConflictReason::kReRegistrationFailed);
		LOG_FATAL << "Node re-registration FAILED for key: " << key
			<< ", node_id=" << gNode->GetNodeInfo().node_id()
			<< ". Another node has claimed this ID — SnowFlake collision is inevitable. "
			   "Active players on this node will be disconnected and must reconnect "
			   "through the normal login flow to be routed to a healthy node. "
			   "This process must terminate now.";
		return;
	}

	if (IsNodeIdKey(key)) {
		acquireNodeTimer.RunAfter(1, [] { NodeAllocator::AcquireNode(); });
		return;
	}

	acquirePortTimer.RunAfter(1, [] { NodeAllocator::AcquireNodePort(); });
}

void EtcdService::StartWatchingPrefixes() {
	for (const auto& prefix : tlsNodeConfigManager.GetBaseDeployConfig().service_discovery_prefixes()) {
		EtcdHelper::StartWatchingPrefix(prefix, revision[prefix]);
		LOG_INFO << "Watching prefix: " << prefix << " from revision " << revision[prefix];
	}
}

void EtcdService::HandlePutEvent(const std::string& key, const std::string& value) {
	// Hijack detection: if another node overwrote our exact node_id key
	// with a different UUID, our identity is stolen.
	const auto &myInfo = gNode->GetNodeInfo();
	const auto myKey = gNode->GetEtcdManager().MakeNodeEtcdKey(myInfo);
	if (key == myKey)
	{
		NodeInfo remoteInfo;
		if (google::protobuf::util::JsonStringToMessage(value, &remoteInfo).ok())
		{
			if (!remoteInfo.node_uuid().empty() &&
				remoteInfo.node_uuid() != myInfo.node_uuid())
			{
				LOG_ERROR << "Node ID hijack detected via Watch! key=" << key
						  << " my_uuid=" << myInfo.node_uuid()
						  << " remote_uuid=" << remoteInfo.node_uuid();
				gNode->OnNodeIdConflictShutdown(NodeIdConflictReason::kReRegistrationFailed);
				LOG_FATAL << "Another node has claimed our node_id=" << myInfo.node_id()
						  << " via etcd Watch. Terminating to prevent SnowFlake collision.";
				return;
			}
		}
	}

	gNode->GetServiceDiscoveryManager().HandleServiceNodeStart(key, value);
}

void EtcdService::HandleDeleteEvent(const std::string& key, const std::string& value) {
	gNode->HandleServiceNodeStop(key, value);
}

void EtcdService::OnWatchResponse(const etcdserverpb::WatchResponse& response) {
	// Registration flow map:
	// 1) Initial boot: Watch ready -> Lease -> Port CAS -> NodeId CAS -> StartRpcServer
	// 2) Re-register: Health monitor detects missing snapshot -> Lease -> Port CAS -> NodeId CAS (same node_id)
	//    If same node_id is already occupied, process exits via LOG_FATAL to protect identity invariants.
	if (!hasSentWatch) {
		RequestNodeLease();
		hasSentWatch = true;
	}

	if (response.canceled()) {
		LOG_WARN << "Watch canceled: " << response.cancel_reason() << ", will re-establish watches";
		ScheduleWatchReconnect();
		return;
	}

	// Track revision for reconnect
	if (response.header().revision() > 0) {
		for (auto& [prefix, rev] : revision) {
			if (response.header().revision() + 1 > rev) {
				rev = response.header().revision() + 1;
			}
		}
	}

	for (const auto& event : response.events()) {
		if (event.type() == mvccpb::Event_EventType::Event_EventType_PUT) {
			HandlePutEvent(event.kv().key(), event.kv().value());
		}
		else if (event.type() == mvccpb::Event_EventType::Event_EventType_DELETE) {
			HandleDeleteEvent(event.kv().key(), event.prev_kv().value());
		}
	}
}

void EtcdService::ScheduleWatchReconnect() {
	static constexpr double kWatchReconnectDelaySec = 2.0;
	watchReconnectTimer.RunAfter(kWatchReconnectDelaySec, [this] {
		LOG_INFO << "Re-establishing etcd watches after stream break";
		StartWatchingPrefixes();
	});
}

void EtcdService::RequestNodeLease() {
	if (leaseRequestInFlight_) {
		LOG_TRACE << "Skip lease request because one is already in flight. mode="
			<< RegistrationModeName(registrationMode_);
		return;
	}

	leaseRequestInFlight_ = true;
	LOG_INFO << "Requesting etcd lease. mode=" << RegistrationModeName(registrationMode_);
	gNode->GetEtcdManager().RequestNodeLease();
}

void EtcdService::StartLeaseKeepAlive() {
	gNode->GetEtcdManager().StartLeaseKeepAlive();
}

void EtcdService::RegisterService() {
	gNode->GetEtcdManager().RegisterNodeService();
}

void EtcdService::Shutdown() {
	grpcHandlerTimer.Cancel();
	acquireNodeTimer.Cancel();
	acquirePortTimer.Cancel();
	watchReconnectTimer.Cancel();
	leaseRequestInFlight_ = false;
	SetRegistrationMode(RegistrationMode::kInitialBoot, "service shutdown");

	auto emptyHandler = [](const ClientContext&, const ::google::protobuf::Message&) {};
	etcdserverpb::AsyncKVRangeHandler = emptyHandler;
	etcdserverpb::AsyncKVPutHandler = emptyHandler;
	etcdserverpb::AsyncKVDeleteRangeHandler = emptyHandler;
	etcdserverpb::AsyncKVTxnHandler = emptyHandler;
	etcdserverpb::AsyncWatchWatchHandler = emptyHandler;
	etcdserverpb::AsyncLeaseLeaseGrantHandler = emptyHandler;

	EtcdHelper::StopAllWatching();
	gNode->GetEtcdManager().Shutdown();
}

void EtcdService::RequestReRegistration() {
	if (registrationMode_ == RegistrationMode::kReRegisterExisting) {
		LOG_DEBUG << "Re-registration already in progress, skipping duplicate attempt.";
		return;
	}
	SetRegistrationMode(RegistrationMode::kReRegisterExisting, "health monitor missing local node snapshot");
	RequestNodeLease();
}

void EtcdService::OnLeaseGranted(const etcdserverpb::LeaseGrantResponse& reply) {
	leaseRequestInFlight_ = false;
	leaseId = reply.id();

	if (leaseId <= 0) {
		LOG_ERROR << "Invalid lease ID received.";
		return;
	}

	leaseTtlSeconds_ = reply.ttl();
	lastKeepAliveAckTime_ = std::chrono::steady_clock::now();
	LOG_INFO << "Lease granted: id=" << leaseId << ", ttl=" << leaseTtlSeconds_ << "s";

	StartLeaseKeepAlive();

	if (registrationMode_ == RegistrationMode::kReRegisterExisting) {
		NodeAllocator::ReRegisterExistingNode();
	} else {
		NodeAllocator::AcquireNodePort(); // Only acquire port initially
	}
}

void EtcdService::OnKeepAliveResponse(const etcdserverpb::LeaseKeepAliveResponse& reply) {
	if (reply.ttl() <= 0) {
		gNode->OnNodeIdConflictShutdown(NodeIdConflictReason::kLeaseExpiredByEtcd);
		LOG_FATAL << "Lease keepalive returned TTL=0, lease has expired on etcd server. "
			"node_id=" << gNode->GetNodeInfo().node_id()
			<< ". Another node may claim this ID — terminating to prevent SnowFlake collision.";
		return;
	}

	lastKeepAliveAckTime_ = std::chrono::steady_clock::now();
	LOG_TRACE << "Lease keepalive ACK, ttl=" << reply.ttl();
}

bool EtcdService::IsLeasePresumablyExpired() const {
	if (leaseTtlSeconds_ <= 0) {
		return false; // Lease not yet granted
	}

	auto elapsed = std::chrono::steady_clock::now() - lastKeepAliveAckTime_;
	auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
	return elapsedSeconds > leaseTtlSeconds_;
}

void EtcdService::ActivateSnowFlakeAfterGuard() {
	const auto& info = gNode->GetNodeInfo();
	std::string guardKey = EtcdManager::MakeSnowFlakeGuardKey(info);

	auto& redis = tlsRedis.GetZoneRedis();
	if (!redis || !redis->connected()) {
		LOG_WARN << "Redis not connected, activating SnowFlake without guard for node_id=" << info.node_id();
		tlsSnowflakeManager.OnNodeStart(info.node_id());
		gNode->StartRpcServer();
		return;
	}

	redis->command(
		[nodeId = info.node_id(), guardKey](hiredis::Hiredis*, redisReply* reply) {
			tlsSnowflakeManager.OnNodeStart(nodeId);

			if (reply != nullptr && reply->type == REDIS_REPLY_STRING) {
				uint64_t lastTs = std::strtoull(reply->str, nullptr, 10);
				uint64_t now = TimeSystem::NowSecondsUTC();
				tlsSnowflakeManager.SetGuardTime(now);
				LOG_INFO << "SnowFlake guard applied: last_ts=" << lastTs
					<< ", guard_to=" << now
					<< ", node_id=" << nodeId
					<< ". Generator will skip current second.";
			} else {
				LOG_INFO << "No SnowFlake guard found for " << guardKey
					<< ", node_id=" << nodeId;
			}

			gNode->StartRpcServer();
		},
		"GET %s", guardKey.c_str());
}

