#include "node.h"
#include <regex>
#include <grpcpp/create_channel.h>
#include <boost/uuid/uuid_io.hpp>
#include "table/code/all_table.h"
#include "node/system/etcd/etcd_helper.h"
#include "config.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/message_differencer.h"
#include "grpc_client/grpc_init_client.h"
#include "grpc_client/etcd/etcd_grpc_client.h"
#include "log/constants/log_constants.h"
#include "log/system/console_log.h"
#include "proto/common/event/server_event.pb.h"
#include "muduo/base/TimeZone.h"
#include "network/process_info.h"
#include "network/rpc_session.h"
#include "node/system/node/node_util.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "proto/common/base/node.pb.h"
#include "proto/common/event/node_event.pb.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "rpc/service_metadata/rpc_event_registry.h"
#include "thread_context/redis_manager.h"
#include "time/system/time.h"
#include "network/network_utils.h"
#include "proto_helpers/proto_util.h"
#include "core/utils/debug/stacktrace_system.h"
#include "network/node_utils.h"
#include <boost/algorithm/string.hpp>
#include "node/system/etcd/etcd_service.h"
#include "node/system/node/node_connector.h"
#include "node/system/node/node_allocator.h"
#include "thread_context/node_context_manager.h"
#include <node_config_manager.h>
#include <thread_context/registry_manager.h>
#include "thread_context/thread_local_entity_container.h"
#include <thread_context/entity_manager.h>
#include <cstdio>
#include <atomic>
#include <future>
#include <chrono>
#include <cstdlib>
#include <cerrno>
#include <optional>

namespace {
std::atomic<Node*> gNodeAtomic{ nullptr };

const char* GetNonEmptyEnv(const char* name) {
	const char* value = std::getenv(name);
	if (value == nullptr || value[0] == '\0') {
		return nullptr;
	}
	return value;
}

std::string ResolveNodeIp() {
	// Prefer K8s Downward API pod IP, then explicit override, then legacy hostname resolve.
	if (const char* podIp = GetNonEmptyEnv("POD_IP")) {
		return podIp;
	}

	if (const char* nodeIp = GetNonEmptyEnv("NODE_IP")) {
		return nodeIp;
	}

	return localip();
}

std::optional<uint16_t> TryResolveNodePortFromEnv() {
	const char* rawPort = GetNonEmptyEnv("RPC_PORT");
	if (rawPort == nullptr) {
		rawPort = GetNonEmptyEnv("NODE_PORT");
	}

	if (rawPort == nullptr) {
		return std::nullopt;
	}

	errno = 0;
	char* endPtr = nullptr;
	const long parsedPort = std::strtol(rawPort, &endPtr, 10);
	if (errno != 0 || endPtr == rawPort || *endPtr != '\0' || parsedPort <= 0 || parsedPort > 65535) {
		LOG_WARN << "Ignore invalid env port value. RPC_PORT/NODE_PORT=" << rawPort;
		return std::nullopt;
	}

	return static_cast<uint16_t>(parsedPort);
}

void StdoutOutput(const char* msg, int len) {
	std::fwrite(msg, 1, static_cast<size_t>(len), stdout);
}

std::string FormatKafkaPartitions(const std::vector<int32_t>& partitions) {
	if (partitions.empty()) {
		return "all";
	}

	std::vector<std::string> partitionTokens;
	partitionTokens.reserve(partitions.size());
	for (int32_t partition : partitions) {
		partitionTokens.emplace_back(std::to_string(partition));
	}

	return boost::algorithm::join(partitionTokens, ",");
}
}

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

Node* gNode;

Node::Node(muduo::net::EventLoop* loop, const std::string& logFilePath)
	: eventLoop(loop), logSystem(logFilePath, kMaxLogFileRollSize, 1) {
	if (eventLoop == nullptr) {
		LOG_FATAL << "Node requires a valid EventLoop pointer.";
	}

	LOG_INFO << "Node created, log file: " << logFilePath;
	if (gNode != nullptr && gNode != this) {
		LOG_FATAL << "Multiple Node instances detected. existing=" << gNode << ", new=" << this;
	}

	gNode = this;
	gNodeAtomic.store(this, std::memory_order_release);
	tlsEcs.nodeGlobalRegistry.emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
}

Node::~Node() {
	Shutdown();
	if (gNode == this) {
		gNodeAtomic.store(nullptr, std::memory_order_release);
		gNode = nullptr;
	}
}

int64_t Node::GetLeaseId() const
{
	return serviceDiscoveryManager.etcdService.GetLeaseId();
}

NodeInfo& Node::GetNodeInfo() const {
	return tlsEcs.globalRegistry.get_or_emplace<NodeInfo>(tlsEcs.GlobalEntity());
}

void Node::Initialize() {
	eventLoop->assertInLoopThread();
	LOG_DEBUG << "Node initializing...";
	SetupTimeZone();
	RegisterHandlers();
	RegisterEventHandlers();
	LoadConfigs();
	InitRpcServer();
	InitLogSystem();
	LoadAllConfigData();
	InitKafka();
	InitEtcdService();
	LOG_DEBUG << "Node initialization complete.";
}

void Node::InitRpcServer() {
	NodeInfo& localNodeInfo = GetNodeInfo();
	const std::string endpointIp = ResolveNodeIp();
	localNodeInfo.mutable_endpoint()->set_ip(endpointIp);

	// Port is resolved later by AcquireNodePort() via etcd, unless overridden by env.
	if (const auto envPort = TryResolveNodePortFromEnv(); envPort) {
		localNodeInfo.mutable_endpoint()->set_port(*envPort);
		LOG_INFO << "Node port from environment: " << *envPort;
	}

	LOG_INFO << "Node endpoint resolved. ip=" << endpointIp;
	localNodeInfo.set_node_type(GetNodeType());
	localNodeInfo.set_scene_node_type(tlsNodeConfigManager.GetGameConfig().scene_node_type());
	localNodeInfo.set_protocol_type(PROTOCOL_TCP);
	localNodeInfo.set_launch_time(TimeSystem::NowMicrosecondsUTC());
	localNodeInfo.set_zone_id(tlsNodeConfigManager.GetGameConfig().zone_id());

	localNodeInfo.set_node_uuid(boost::uuids::to_string(uuidGenerator()));

	muduo::net::InetAddress zoneRedisAddress(tlsNodeConfigManager.GetGameConfig().zone_redis().host(), tlsNodeConfigManager.GetGameConfig().zone_redis().port());
	tlsRedis.GetZoneRedis() = std::make_unique<RedisManager::HiredisPtr::element_type>(eventLoop, zoneRedisAddress);
	tlsRedis.GetZoneRedis()->connect();

	LOG_DEBUG << "Node info: " << localNodeInfo.DebugString();
}

void Node::InitKafka()
{
	kafkaManager.Init(tlsNodeConfigManager.GetBaseDeployConfig().kafka());
}

void Node::StartKafkaPolling()
{
	kafkaConsumerTimer.RunEvery(0.1, [this] { kafkaManager.Poll(); });
}

bool Node::RegisterKafkaMessageHandler(const std::vector<std::string>& topics,
	const std::string& groupId,
	KafkaMessageHandler handler,
	const std::vector<int32_t>& partitions)
{
	if (topics.empty()) {
		LOG_ERROR << "RegisterKafkaMessageHandler failed: topics is empty.";
		return false;
	}

	if (!handler) {
		LOG_ERROR << "RegisterKafkaMessageHandler failed: handler is null.";
		return false;
	}

	auto& kafkaConfig = tlsNodeConfigManager.GetBaseDeployConfig().kafka();
	if (!GetKafkaManager().Subscribe(kafkaConfig, topics, groupId, partitions, std::move(handler))) {
		LOG_ERROR << "Kafka subscribe failed. group_id=" << groupId;
		return false;
	}

	LOG_INFO << "Kafka subscribe succeeded. group_id=" << groupId
		<< ", topics=" << boost::algorithm::join(topics, ",")
		<< ", partitions=" << FormatKafkaPartitions(partitions);

	if (!kafkaPollingStarted) {
		StartKafkaPolling();
		kafkaPollingStarted = true;
	}

	return true;
}

void Node::InitEtcdService()
{
	serviceDiscoveryManager.Init();
}

void Node::OnNodeIdConflictShutdown(NodeIdConflictReason reason) {
	LOG_WARN << "Node identity conflict detected (reason="
		<< static_cast<int>(reason) << "), node_id=" << GetNodeId()
		<< ". Override OnNodeIdConflictShutdown() to flush players before termination.";
	// Base implementation: no-op. Subclasses (SceneNode, GateNode, etc.)
	// should override to: save player data, migrate/kick players, etc.
	// For instance nodes: notify players that the dungeon is lost.
}

void Node::StartRpcServer() {
	eventLoop->assertInLoopThread();
	if (rpcServer) {
		LOG_TRACE << "RPC server already started, skipping.";
		return;
	}

	NodeInfo& localNodeInfo = GetNodeInfo();
	muduo::net::InetAddress rpcListenAddress(localNodeInfo.endpoint().ip(), localNodeInfo.endpoint().port());

	rpcServer = std::make_unique<RpcServerPtr::element_type>(eventLoop, rpcListenAddress);
	rpcServer->start();
	auto* nodeReplyService = GetNodeReplyService();
	if (nodeReplyService != nullptr) {
		rpcServer->registerService(nodeReplyService);
	}
	else {
		LOG_WARN << "Node reply service is null, skip registerService for node_type=" << GetNodeInfo().node_type();
	}

	for (auto& [serviceName, service] : gNodeService) {
		if (service == nullptr) {
			LOG_WARN << "Skip null node service registration for key=" << serviceName;
			continue;
		}

		rpcServer->registerService(service.get());
	}

	NodeConnector::ConnectAllNodes();

	if (!RegisterKafkaHandlers()) {
		LOG_FATAL << "RegisterKafkaHandlers failed for node_type=" << GetNodeInfo().node_type();
	}

	StartNodeRegistrationHealthMonitor();

	tlsEcs.dispatcher.trigger<OnServerStart>();

	auto nodeTypeName = boost::to_upper_copy(eNodeType_Name(GetNodeInfo().node_type()));
	LOG_INFO << "\n\n"
		<< "=============================================================\n"
		<< "	" << nodeTypeName << " NODE STARTED SUCCESSFULLY\n"
		<< "	Node Info:\n" << GetNodeInfo().DebugString() << "\n"
		<< "=============================================================\n";
}

void Node::Shutdown() {
	if (eventLoop == nullptr) {
		return;
	}

	if (eventLoop->isInLoopThread()) {
		ShutdownInLoop();
		return;
	}

	std::promise<void> shutdownPromise;
	auto shutdownFuture = shutdownPromise.get_future();
	eventLoop->runInLoop([this, &shutdownPromise]() {
		ShutdownInLoop();
		shutdownPromise.set_value();
	});
	constexpr auto kShutdownWaitTimeout = std::chrono::seconds(5);
	if (shutdownFuture.wait_for(kShutdownWaitTimeout) != std::future_status::ready) {
		LOG_ERROR << "Node shutdown timed out waiting for loop thread. timeout_s="
			<< std::chrono::duration_cast<std::chrono::seconds>(kShutdownWaitTimeout).count();
	}
}

void Node::ShutdownInLoop() {
	eventLoop->assertInLoopThread();
	if (shutdownStarted.exchange(true, std::memory_order_acq_rel)) {
		return;
	}

	LOG_DEBUG << "Node shutting down...";
	grpcHandlerTimer.Cancel();
	serviceHealthMonitorTimer.Cancel();
	acquireNodeTimer.Cancel();
	acquirePortTimer.Cancel();
	kafkaProducerTimer.Cancel();
	kafkaConsumerTimer.Cancel();
	ReleaseNodeId();
	serviceDiscoveryManager.Shutdown();
	kafkaManager.Shutdown();
	// Cleared by tlsEcs.Clear() below.
	tlsEcs.Clear();
	muduo::Logger::setOutput(StdoutOutput);
	logSystem.stop();
	LOG_DEBUG << "Node shutdown complete.";
}

void Node::InitLogSystem() {
	auto logLevel = static_cast<muduo::Logger::LogLevel>(
		tlsNodeConfigManager.GetBaseDeployConfig().log_level()
		);
	muduo::Logger::setLogLevel(logLevel);
	muduo::Logger::setOutput(AsyncOutput);
	logSystem.start();
}

void Node::RegisterEventHandlers() {
	tlsEcs.dispatcher.sink<OnConnected2TcpServerEvent>().connect<&Node::OnServerConnected>(*this);
}

void Node::LoadConfigs() {
	tlsNodeConfigManager = NodeConfigManager();
	readBaseDeployConfig("etc/base_deploy_config.yaml", tlsNodeConfigManager.GetBaseDeployConfig());
	readGameConfig("etc/game_config.yaml", tlsNodeConfigManager.GetGameConfig());
	gNodeConfigManager = tlsNodeConfigManager;
}

void Node::LoadAllConfigData() {
	LoadTablesAsync();
}

void Node::SetupTimeZone() {
	const muduo::TimeZone hkTz("zoneinfo/Asia/Hong_Kong");
	muduo::Logger::setTimeZone(hkTz);
}

void Node::StopWatchingServiceNodes() {
	EtcdHelper::StopAllWatching();
}

void Node::ReleaseNodeId() {
	EtcdHelper::RevokeLeaseAndCleanup(serviceDiscoveryManager.etcdService.GetLeaseId());
}

// These functions are provided by each node binary; the linker resolves them.
void InitReply();
void InitPlayerService();
void InitPlayerServiceReplied();
void InitServiceHandler();
void Node::RegisterHandlers() {
	InitMessageInfo();
	InitReply();
	InitPlayerService();
	InitPlayerServiceReplied();
	InitServiceHandler();
}

void Node::AsyncOutput(const char* msg, int len) {
	Node* activeNode = gNodeAtomic.load(std::memory_order_acquire);
	if (activeNode != nullptr) {
		activeNode->Log().append(msg, len);
		return;
	}
	StdoutOutput(msg, len);
#ifdef WIN32
	LogToConsole(msg, len);
#endif
}

std::string Node::FormatIpAndPort() {
	return ::FormatIpAndPort(GetNodeInfo().endpoint().ip(), GetNodeInfo().endpoint().port());
}

std::string Node::GetIp() {
	return GetNodeInfo().endpoint().ip();
}

uint32_t Node::GetPort() {
	return GetNodeInfo().endpoint().port();
}


bool Node::IsCurrentNode(const NodeInfo& candidateNode) const
{
	return NodeUtils::IsSameNode(candidateNode.node_uuid(), GetNodeInfo().node_uuid());
}

void Node::HandleServiceNodeStop(const std::string& key, const std::string& nodeJson) {
	eventLoop->assertInLoopThread();
	LOG_INFO << "Service node stop, key: " << key << ", value: " << nodeJson;

	NodeInfo stoppedNode;
	auto parseResult = google::protobuf::util::JsonStringToMessage(nodeJson, &stoppedNode);
	if (!parseResult.ok()) {
		LOG_ERROR << "Parse node JSON failed, key: " << key
			<< ", JSON: " << nodeJson
			<< ", Error: " << parseResult.message().data();
		return;
	}

	if (!eNodeType_IsValid(stoppedNode.node_type())) {
		LOG_TRACE << "Unknown service type for key: " << key;
		return;
	}

	if (stoppedNode.node_uuid().empty()) {
		LOG_WARN << "Ignore service node stop with empty node_uuid. key=" << key;
		return;
	}

	auto& serviceNodesByType = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
	auto& nodesOfStoppedType = *serviceNodesByType[stoppedNode.node_type()].mutable_node_list();

	// Remove stale node snapshot first so service discovery state stays consistent.
	int removedSnapshotCount = 0;
	for (int i = nodesOfStoppedType.size() - 1; i >= 0; --i) {
		const auto& cachedNodeSnapshot = nodesOfStoppedType.Get(i);
		if (!NodeUtils::IsSameNode(cachedNodeSnapshot.node_uuid(), stoppedNode.node_uuid())) {
			continue;
		}

		nodesOfStoppedType.DeleteSubrange(i, 1);
		++removedSnapshotCount;
	}

	if (removedSnapshotCount == 0) {
		LOG_WARN << "Service node stop did not match local cache. node_id=" << stoppedNode.node_id()
			<< ", node_uuid=" << stoppedNode.node_uuid()
			<< ", node_type=" << stoppedNode.node_type();
	}
	else {
		LOG_INFO << "Removed " << removedSnapshotCount << " stale node record(s) for uuid=" << stoppedNode.node_uuid();
	}

	const auto stoppedNodeType = stoppedNode.node_type();
	const auto stoppedNodeId = stoppedNode.node_id();
	const auto stoppedNodeUuid = stoppedNode.node_uuid();

	// Important: destroy network entities after current channel dispatch cycle.
	// Direct destroy inside etcd watch callback can invalidate activeChannels_.
	GetLoop()->queueInLoop([stoppedNodeType, stoppedNodeId, stoppedNodeUuid]() {
		auto stoppedNodeEntity = entt::entity{ stoppedNodeId };
		entt::registry& nodeRegistry = tlsNodeContextManager.GetRegistry(stoppedNodeType);

		if (nodeRegistry.valid(stoppedNodeEntity)) {
			auto* currentNodeInfo = nodeRegistry.try_get<NodeInfo>(stoppedNodeEntity);
			if (currentNodeInfo && !NodeUtils::IsSameNode(currentNodeInfo->node_uuid(), stoppedNodeUuid)) {
				LOG_WARN << "Skip node destroy due to node id reuse. node_id=" << stoppedNodeId
					<< ", stale_uuid=" << stoppedNodeUuid
					<< ", current_uuid=" << currentNodeInfo->node_uuid();
				return;
			}
		}

		OnNodeRemovePbEvent nodeRemovedEvent;
		nodeRemovedEvent.set_entity(entt::to_integral(stoppedNodeEntity));
		nodeRemovedEvent.set_node_type(stoppedNodeType);
		tlsEcs.dispatcher.trigger(nodeRemovedEvent);

		DestroyEntity(nodeRegistry, stoppedNodeEntity);
	});
	LOG_INFO << "Service node stopped : " << stoppedNode.DebugString();
}

void Node::OnServerConnected(const OnConnected2TcpServerEvent& connectedEvent) {
	eventLoop->assertInLoopThread();
	if (rpcServer == nullptr) {
		return;
	}

	auto& connection = connectedEvent.conn_;
	if (!connection->connected()) {
		LOG_INFO << "Client disconnected: " << connection->peerAddress().toIpPort();
		return;
	}
	LOG_INFO << "Connected to server: " << connection->peerAddress().toIpPort();
	for (uint32_t nodeType = 0; nodeType < eNodeType_ARRAYSIZE; ++nodeType)
	{
		nodeRegistrationManager.TryRegisterNodeSession(nodeType, connection);
	}
}

void Node::StartNodeRegistrationHealthMonitor() {
	serviceHealthMonitorTimer.RunEvery(tlsNodeConfigManager.GetBaseDeployConfig().health_check_interval(),
		[this, reRegistrationRequested = false]() mutable {
		if (rpcServer == nullptr)
		{
			return;
		}

		// Check lease deadline first — this catches network partitions where
		// the watch stream is dead and the local ServiceNodeList is stale.
		if (serviceDiscoveryManager.etcdService.IsLeasePresumablyExpired()) {
			OnNodeIdConflictShutdown(NodeIdConflictReason::kLeaseDeadlineExceeded);
			LOG_FATAL << "Lease deadline exceeded: no keepalive ACK from etcd within TTL. "
				"node_id=" << GetNodeInfo().node_id()
				<< ". Etcd has likely expired our lease; another node may claim this ID. "
				   "Terminating to prevent SnowFlake collision. "
				   "Active players will reconnect through the normal login flow.";
			return;
		}

		auto& currentNode = GetNodeInfo();

		auto& serviceNodesByType = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
		auto& registeredNodesForType = *serviceNodesByType[currentNode.node_type()].mutable_node_list();
		for (const auto& registeredNode : registeredNodesForType) {
			if (IsCurrentNode(registeredNode)) {
				reRegistrationRequested = false;
				return;
			}
		}

		// Node snapshot disappeared from service discovery.
		// Re-register with the same node_id; do not run full re-allocation flow.
		if (reRegistrationRequested) {
			return;
		}

		serviceDiscoveryManager.etcdService.RequestReRegistration();
		reRegistrationRequested = true;
		}
	);
}



