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
#include "core/utils/debug/stacktrace_system.h"
#include "network/node_utils.h"
#include "node/system/node/thread_observability.h"
#include "network/traffic_statistics.h"
#include <boost/algorithm/string.hpp>
#include "node/system/etcd/etcd_service.h"
#include "node/system/node/node_connector.h"
#include "thread_context/node_context_manager.h"
#include <node_config_manager.h>
#include "thread_context/thread_local_entity_container.h"
#include <atomic>
#include <future>
#include <chrono>
#include <cerrno>
#include <optional>

namespace
{
	std::atomic<Node *> gNodeAtomic{nullptr};

	const char *GetNonEmptyEnv(const char *name)
	{
		const char *value = std::getenv(name);
		if (value == nullptr || value[0] == '\0')
		{
			return nullptr;
		}
		return value;
	}

	std::string ResolveNodeIp()
	{
		// Prefer K8s Downward API pod IP, then explicit override, then legacy hostname resolve.
		if (const char *podIp = GetNonEmptyEnv("POD_IP"))
		{
			return podIp;
		}

		if (const char *nodeIp = GetNonEmptyEnv("NODE_IP"))
		{
			return nodeIp;
		}

		return localip();
	}

	std::optional<uint16_t> TryResolveNodePortFromEnv()
	{
		const char *rawPort = GetNonEmptyEnv("RPC_PORT");
		if (rawPort == nullptr)
		{
			rawPort = GetNonEmptyEnv("NODE_PORT");
		}

		if (rawPort == nullptr)
		{
			return std::nullopt;
		}

		errno = 0;
		char *endPtr = nullptr;
		const long parsedPort = std::strtol(rawPort, &endPtr, 10);
		if (errno != 0 || endPtr == rawPort || *endPtr != '\0' || parsedPort <= 0 || parsedPort > 65535)
		{
			LOG_WARN << "Ignore invalid env port value. RPC_PORT/NODE_PORT=" << rawPort;
			return std::nullopt;
		}

		return static_cast<uint16_t>(parsedPort);
	}

	std::string FormatKafkaPartitions(const std::vector<int32_t> &partitions)
	{
		if (partitions.empty())
		{
			return "all";
		}

		std::vector<std::string> partitionTokens;
		partitionTokens.reserve(partitions.size());
		for (int32_t partition : partitions)
		{
			partitionTokens.emplace_back(std::to_string(partition));
		}

		return boost::algorithm::join(partitionTokens, ",");
	}
}

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

Node *gNode;

Node::Node(muduo::net::EventLoop *loop, const std::string &logFilePath)
	: eventLoop(loop), logSystem(logFilePath, kMaxLogFileRollSize, 1)
{
	if (eventLoop == nullptr)
	{
		LOG_FATAL << "Node requires a valid EventLoop pointer.";
	}

	// Start async log system early so all constructor logs have correct formatting/colors.
	muduo::Logger::setOutput(AsyncOutput);
	logSystem.start();

	// Set timezone before any LOG_xxx call. Without a valid timezone, muduo appends 'Z'
	// to the timestamp (9 bytes instead of 8), shifting the log-level character past
	// kLoginInfoInex and causing LogToConsole to fall into the default (red) color.
	SetupTimeZone();

	LOG_INFO << "Node created, log file: " << logFilePath;
	if (gNode != nullptr && gNode != this)
	{
		LOG_FATAL << "Multiple Node instances detected. existing=" << gNode << ", new=" << this;
	}

	gNode = this;
	gNodeAtomic.store(this, std::memory_order_release);
	tlsEcs.nodeGlobalRegistry.emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
}

Node::Node(muduo::net::EventLoop *loop,
		   uint32_t nodeType,
		   CanConnectNodeTypeList connectTo,
		   ::google::protobuf::Service *replyService)
	: Node(loop, "logs/cpp_nodes/" + NodeUtils::NodeTypeToShortName(nodeType))
{
	replyService_ = replyService;
	GetNodeInfo().set_node_type(nodeType);
	targetNodeTypeWhitelist = std::move(connectTo);
	Initialize();
	node::observability::RegisterThreadObservability(*eventLoop, "logs/" + NodeUtils::NodeTypeToShortName(nodeType));
	RegisterTrafficStatsReporter(*eventLoop);
}

Node::~Node()
{
	Shutdown();
	if (gNode == this)
	{
		gNodeAtomic.store(nullptr, std::memory_order_release);
		gNode = nullptr;
	}
}

int64_t Node::GetLeaseId() const
{
	return serviceDiscoveryManager.etcdService.GetLeaseId();
}

NodeInfo &Node::GetNodeInfo() const
{
	return tlsEcs.globalRegistry.get_or_emplace<NodeInfo>(tlsEcs.GlobalEntity());
}

void Node::Initialize()
{
	eventLoop->assertInLoopThread();
	LOG_DEBUG << "Node initializing...";
	RegisterHandlers();
	RegisterEventHandlers();
	LoadConfigs();
	InitLogSystem();
	InitRpcServer();
	LoadAllConfigData();
	InitKafka();
	InitEtcdService();

	LOG_INFO << "gRPC client config: ResourceQuota max threads=" << grpc_channel_cache::ConfiguredMaxThreads()
			 << ", backup poll interval ms=" << grpc_channel_cache::ConfiguredBackupPollIntervalMs()
			 << ", EventEngine pool reserve=" << (grpc_channel_cache::ConfiguredThreadPoolReserveThreads() > 0 ? std::to_string(grpc_channel_cache::ConfiguredThreadPoolReserveThreads()) : std::string("default"))
			 << ", EventEngine pool max=" << (grpc_channel_cache::ConfiguredThreadPoolMaxThreads() > 0 ? std::to_string(grpc_channel_cache::ConfiguredThreadPoolMaxThreads()) : std::string("unlimited"));

	LOG_DEBUG << "Node initialization complete.";
}

void Node::InitRpcServer()
{
	NodeInfo &localNodeInfo = GetNodeInfo();
	const std::string endpointIp = ResolveNodeIp();
	localNodeInfo.mutable_endpoint()->set_ip(endpointIp);

	// Port is resolved later by AcquireNodePort() via etcd, unless overridden by env.
	if (const auto envPort = TryResolveNodePortFromEnv(); envPort)
	{
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

	const auto &redisHost = tlsNodeConfigManager.GetGameConfig().zone_redis().host();
	const auto redisPort = static_cast<uint16_t>(tlsNodeConfigManager.GetGameConfig().zone_redis().port());
	muduo::net::InetAddress zoneRedisAddress(redisPort);

	// If the host looks like an IP address, use it directly without DNS resolve
	bool isIpLiteral = !redisHost.empty() && (std::isdigit(static_cast<unsigned char>(redisHost[0])) || redisHost.find(':') != std::string::npos);
	if (isIpLiteral)
	{
		zoneRedisAddress = muduo::net::InetAddress(redisHost, redisPort);
	}
	else if (!muduo::net::InetAddress::resolve(redisHost, &zoneRedisAddress))
	{
		LOG_WARN << "DNS resolve failed for Redis host '" << redisHost << "', treating as literal IP";
		zoneRedisAddress = muduo::net::InetAddress(redisHost, redisPort);
	}
	LOG_INFO << "Zone Redis address: " << zoneRedisAddress.toIpPort();
	tlsRedis.GetZoneRedis() = std::make_unique<RedisManager::HiredisPtr::element_type>(eventLoop, zoneRedisAddress);
	tlsRedis.GetZoneRedis()->connect();
	tlsRedis.SetupReconnect(eventLoop, zoneRedisAddress);

	LOG_DEBUG << "Node info: " << localNodeInfo.DebugString();
}

void Node::InitKafka()
{
	kafkaManager.Init(tlsNodeConfigManager.GetBaseDeployConfig().kafka());
}

void Node::StartKafkaPolling()
{
	kafkaConsumerTimer.RunEvery(0.1, [this]
								{ kafkaManager.Poll(); });
}

bool Node::RegisterKafkaMessageHandler(const std::vector<std::string> &topics,
									   const std::string &groupId,
									   KafkaMessageHandler handler,
									   const std::vector<int32_t> &partitions)
{
	if (topics.empty())
	{
		LOG_ERROR << "RegisterKafkaMessageHandler failed: topics is empty.";
		return false;
	}

	if (!handler)
	{
		LOG_ERROR << "RegisterKafkaMessageHandler failed: handler is null.";
		return false;
	}

	auto &kafkaConfig = tlsNodeConfigManager.GetBaseDeployConfig().kafka();
	if (!GetKafkaManager().Subscribe(kafkaConfig, topics, groupId, partitions, std::move(handler)))
	{
		LOG_ERROR << "Kafka subscribe failed. group_id=" << groupId;
		return false;
	}

	LOG_INFO << "Kafka subscribe succeeded. group_id=" << groupId
			 << ", topics=" << boost::algorithm::join(topics, ",")
			 << ", partitions=" << FormatKafkaPartitions(partitions);

	if (!kafkaPollingStarted)
	{
		StartKafkaPolling();
		kafkaPollingStarted = true;
	}

	return true;
}

void Node::InitEtcdService()
{
	serviceDiscoveryManager.Init();
}

void Node::RegisterGrpcService(grpc::Service *service)
{
	if (service == nullptr)
	{
		LOG_WARN << "Attempted to register null gRPC service, skipping.";
		return;
	}
	grpcServices_.push_back(service);
}

void Node::StartGrpcServer()
{
	if (grpcServices_.empty())
	{
		return;
	}

	const auto &grpcEp = GetNodeInfo().grpc_endpoint();
	if (grpcEp.port() == 0)
	{
		LOG_ERROR << "gRPC server port not allocated, skipping gRPC server start.";
		return;
	}

	const std::string serverAddress = grpcEp.ip() + ":" + std::to_string(grpcEp.port());

	grpc::ServerBuilder builder;
	builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());

	// Limit gRPC sync server thread pool: control-plane RPCs (CreateScene, DestroyScene)
	// are low-frequency and dispatched to the muduo loop anyway, so 1-2 pollers suffice.
	int maxPollers = 2;
	if (const char *env = GetNonEmptyEnv("GRPC_SERVER_MAX_POLLERS"))
	{
		const int parsed = std::atoi(env);
		if (parsed >= 1)
		{
			maxPollers = parsed;
		}
	}
	builder.SetSyncServerOption(grpc::ServerBuilder::NUM_CQS, 1);
	builder.SetSyncServerOption(grpc::ServerBuilder::MIN_POLLERS, 1);
	builder.SetSyncServerOption(grpc::ServerBuilder::MAX_POLLERS, maxPollers);
	LOG_INFO << "gRPC server config: max_pollers=" << maxPollers;

	for (auto *svc : grpcServices_)
	{
		builder.RegisterService(svc);
	}

	grpcServer_ = builder.BuildAndStart();
	if (!grpcServer_)
	{
		LOG_FATAL << "Failed to start gRPC server on " << serverAddress;
		return;
	}

	grpcServerThread_ = std::thread([this, serverAddress]
									{
		LOG_INFO << "gRPC server thread started, listening on " << serverAddress;
		grpcServer_->Wait(); });

	LOG_INFO << "gRPC server started on " << serverAddress;
}

void Node::ShutdownGrpcServer()
{
	if (grpcServer_)
	{
		LOG_INFO << "Shutting down gRPC server...";
		grpcServer_->Shutdown();
		if (grpcServerThread_.joinable())
		{
			grpcServerThread_.join();
		}
		grpcServer_.reset();
		LOG_INFO << "gRPC server shut down.";
	}
}

void Node::OnNodeIdConflictShutdown(NodeIdConflictReason reason)
{
	LOG_WARN << "Node identity conflict detected (reason="
			 << static_cast<int>(reason) << "), node_id=" << GetNodeId();

	if (onConflictShutdownFn_)
	{
		LOG_INFO << "Running conflict-shutdown hook before termination...";
		onConflictShutdownFn_(*this, reason);
	}
}

void Node::StartRpcServer()
{
	eventLoop->assertInLoopThread();
	if (rpcServer)
	{
		LOG_TRACE << "RPC server already started, skipping.";
		return;
	}

	NodeInfo &localNodeInfo = GetNodeInfo();
	muduo::net::InetAddress rpcListenAddress(localNodeInfo.endpoint().ip(), localNodeInfo.endpoint().port());

	rpcServer = std::make_unique<RpcServerPtr::element_type>(eventLoop, rpcListenAddress);
	rpcServer->start();
	auto *nodeReplyService = GetNodeReplyService();
	if (nodeReplyService != nullptr)
	{
		rpcServer->registerService(nodeReplyService);
	}
	else
	{
		LOG_WARN << "Node reply service is null, skip registerService for node_type=" << GetNodeInfo().node_type();
	}

	for (auto &[serviceName, service] : gNodeService)
	{
		if (service == nullptr)
		{
			LOG_WARN << "Skip null node service registration for key=" << serviceName;
			continue;
		}

		rpcServer->registerService(service.get());
	}

	NodeConnector::ConnectAllNodes();

	if (!RegisterKafkaHandlers())
	{
		LOG_FATAL << "RegisterKafkaHandlers failed for node_type=" << GetNodeInfo().node_type();
	}

	StartNodeRegistrationHealthMonitor();

	StartGrpcServer();

	tlsEcs.dispatcher.trigger<OnServerStart>();

	auto nodeTypeName = boost::to_upper_copy(eNodeType_Name(GetNodeInfo().node_type()));
	const auto &ep = GetNodeInfo().endpoint();
	const auto &deployConfig = tlsNodeConfigManager.GetBaseDeployConfig();
	const auto &gameConfig = tlsNodeConfigManager.GetGameConfig();

	// Build connects-to list
	std::string connectsTo;
	for (auto nodeType : targetNodeTypeWhitelist)
	{
		if (!connectsTo.empty())
			connectsTo += ", ";
		connectsTo += eNodeType_Name(nodeType);
	}
	if (connectsTo.empty())
		connectsTo = "(none)";

	// Build Kafka brokers string
	std::string kafkaBrokers = boost::algorithm::join(
		std::vector<std::string>(deployConfig.kafka().brokers().begin(), deployConfig.kafka().brokers().end()), ", ");
	if (kafkaBrokers.empty())
		kafkaBrokers = "(not configured)";

	// Build etcd hosts string
	std::string etcdHosts = boost::algorithm::join(
		std::vector<std::string>(deployConfig.etcd_hosts().begin(), deployConfig.etcd_hosts().end()), ", ");
	if (etcdHosts.empty())
		etcdHosts = "(not configured)";

	// Build gRPC listen string
	std::string grpcListen = "(disabled)";
	if (GetNodeInfo().grpc_endpoint().port() > 0)
	{
		grpcListen = GetNodeInfo().grpc_endpoint().ip() + ":" + std::to_string(GetNodeInfo().grpc_endpoint().port());
	}

	// Print startup banner to both log file and stdout/console so it is always visible.
	const std::string banner =
		"\n\n"
		"=============================================================\n"
		"  " +
		nodeTypeName + " NODE STARTED SUCCESSFULLY\n"
					   "=============================================================\n"
					   "  Listen:      " +
		ep.ip() + ":" + std::to_string(ep.port()) + "\n"
													"  gRPC:        " +
		grpcListen + "\n"
					 "  node_id:     " +
		std::to_string(GetNodeId()) + "\n"
									  "  node_uuid:   " +
		GetNodeInfo().node_uuid() + "\n"
									"  zone_id:     " +
		std::to_string(gameConfig.zone_id()) + "\n"
											   "  etcd:        " +
		etcdHosts + "\n"
					"  kafka:       " +
		kafkaBrokers + "\n"
					   "  redis:       " +
		gameConfig.zone_redis().host() + ":" + std::to_string(gameConfig.zone_redis().port()) + "\n"
																								"  connects_to: " +
		connectsTo + "\n"
					 "  log_level:   " +
		std::to_string(deployConfig.log_level()) + "\n"
												   "=============================================================\n";
	LOG_INFO << banner;

	if (afterStartFn_)
		afterStartFn_(*this);
}

void Node::Shutdown()
{
	if (eventLoop == nullptr)
	{
		return;
	}

	if (eventLoop->isInLoopThread())
	{
		ShutdownInLoop();
		return;
	}

	std::promise<void> shutdownPromise;
	auto shutdownFuture = shutdownPromise.get_future();
	eventLoop->runInLoop([this, &shutdownPromise]()
						 {
		ShutdownInLoop();
		shutdownPromise.set_value(); });
	constexpr auto kShutdownWaitTimeout = std::chrono::seconds(5);
	if (shutdownFuture.wait_for(kShutdownWaitTimeout) != std::future_status::ready)
	{
		LOG_ERROR << "Node shutdown timed out waiting for loop thread. timeout_s="
				  << std::chrono::duration_cast<std::chrono::seconds>(kShutdownWaitTimeout).count();
	}
}

void Node::ShutdownInLoop()
{
	eventLoop->assertInLoopThread();
	if (shutdownStarted.exchange(true, std::memory_order_acq_rel))
	{
		return;
	}

	LOG_DEBUG << "Node shutting down...";

	if (beforeShutdownFn_)
	{
		LOG_INFO << "Running before-shutdown hook...";
		beforeShutdownFn_(*this);
	}

	ShutdownGrpcServer();
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
#ifdef WIN32
	muduo::Logger::setOutput(LogToConsole);
#endif
	logSystem.stop();
	LOG_DEBUG << "Node shutdown complete.";

	eventLoop->quit();
}

void Node::InitLogSystem()
{
	// Log system already started in constructor; just apply config-driven log level.
	auto logLevel = static_cast<muduo::Logger::LogLevel>(
		tlsNodeConfigManager.GetBaseDeployConfig().log_level());
	muduo::Logger::setLogLevel(logLevel);
}

void Node::RegisterEventHandlers()
{
	tlsEcs.dispatcher.sink<OnConnected2TcpServerEvent>().connect<&Node::OnServerConnected>(*this);
}

void Node::LoadConfigs()
{
	tlsNodeConfigManager = NodeConfigManager();
	readBaseDeployConfig("etc/base_deploy_config.yaml", tlsNodeConfigManager.GetBaseDeployConfig());
	readGameConfig("etc/game_config.yaml", tlsNodeConfigManager.GetGameConfig());
	gNodeConfigManager = tlsNodeConfigManager;
}

void Node::LoadAllConfigData()
{
	LoadTablesAsync();
}

void Node::SetupTimeZone()
{
#ifdef __linux__
	const muduo::TimeZone hkTz = muduo::TimeZone::loadZoneFile("zoneinfo/Asia/Hong_Kong");
#else
	const muduo::TimeZone hkTz(8 * 3600, "zoneinfo/Asia/Hong_Kong");
#endif // __linux__
	muduo::Logger::setTimeZone(hkTz);
}

void Node::StopWatchingServiceNodes()
{
	EtcdHelper::StopAllWatching();
}

void Node::ReleaseNodeId()
{
	EtcdHelper::RevokeLeaseAndCleanup(serviceDiscoveryManager.etcdService.GetLeaseId());
}

// These functions are provided by each node binary; the linker resolves them.
void InitReply();
void InitPlayerService();
void InitPlayerServiceReplied();
void InitServiceHandler();
void Node::RegisterHandlers()
{
	InitMessageInfo();
	InitReply();
	InitPlayerService();
	InitPlayerServiceReplied();
	InitServiceHandler();
}

void Node::AsyncOutput(const char *msg, int len)
{
	Node *activeNode = gNodeAtomic.load(std::memory_order_acquire);
	if (activeNode != nullptr)
	{
		activeNode->Log().append(msg, len);
	}
#ifdef WIN32
	LogToConsole(msg, len);
#endif
}

bool Node::IsCurrentNode(const NodeInfo &candidateNode) const
{
	return NodeUtils::IsSameNode(candidateNode.node_uuid(), GetNodeInfo().node_uuid());
}

void Node::HandleServiceNodeStop(const std::string &key, const std::string &nodeJson)
{
	eventLoop->assertInLoopThread();
	LOG_INFO << "Service node stop, key: " << key << ", value: " << nodeJson;

	NodeInfo stoppedNode;
	auto parseResult = google::protobuf::util::JsonStringToMessage(nodeJson, &stoppedNode);
	if (!parseResult.ok())
	{
		LOG_ERROR << "Parse node JSON failed, key: " << key
				  << ", JSON: " << nodeJson
				  << ", Error: " << parseResult.message().data();
		return;
	}

	if (!eNodeType_IsValid(stoppedNode.node_type()))
	{
		LOG_TRACE << "Unknown service type for key: " << key;
		return;
	}

	if (stoppedNode.node_uuid().empty())
	{
		LOG_WARN << "Ignore service node stop with empty node_uuid. key=" << key;
		return;
	}

	const auto graceSeconds = tlsNodeConfigManager.GetBaseDeployConfig().node_removal_grace_seconds();

	// --- Grace period path: defer removal so breakpoint-paused nodes can re-register ---
	if (graceSeconds > 0)
	{
		const auto &nodeUuid = stoppedNode.node_uuid();

		// Already pending? Reset timer.
		if (pendingNodeRemovals_.count(nodeUuid))
		{
			LOG_INFO << "Node removal already pending, resetting grace timer. uuid=" << nodeUuid;
			pendingNodeRemovals_.erase(nodeUuid);
		}

		auto pending = std::make_unique<PendingNodeRemoval>();
		pending->nodeInfo.CopyFrom(stoppedNode);

		// Capture uuid by value for the timer callback.
		std::string capturedUuid = nodeUuid;
		pending->timer.RunAfter(static_cast<double>(graceSeconds), [this, capturedUuid]()
								{
			auto it = pendingNodeRemovals_.find(capturedUuid);
			if (it == pendingNodeRemovals_.end())
			{
				return; // Already cancelled by a re-register PUT event.
			}

			LOG_WARN << "Grace period expired, removing node. uuid=" << capturedUuid;
			NodeInfo expiredNode;
			expiredNode.CopyFrom(it->second->nodeInfo);
			pendingNodeRemovals_.erase(it);

			// Execute the actual removal (same logic as the immediate path below).
			ExecuteNodeRemoval(expiredNode); });

		pendingNodeRemovals_[nodeUuid] = std::move(pending);
		LOG_INFO << "Node removal deferred for " << graceSeconds << "s grace period. uuid=" << nodeUuid;
		return;
	}

	// --- Immediate removal path (production: grace_seconds == 0) ---
	ExecuteNodeRemoval(stoppedNode);
}

void Node::CancelPendingNodeRemoval(const std::string &nodeUuid)
{
	auto it = pendingNodeRemovals_.find(nodeUuid);
	if (it == pendingNodeRemovals_.end())
	{
		return;
	}

	LOG_INFO << "Node re-registered during grace period, cancelling pending removal. uuid=" << nodeUuid;
	it->second->timer.Cancel();
	pendingNodeRemovals_.erase(it);
}

void Node::ExecuteNodeRemoval(const NodeInfo &stoppedNode)
{
	auto &serviceNodesByType = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
	auto &nodesOfStoppedType = *serviceNodesByType[stoppedNode.node_type()].mutable_node_list();

	// Remove stale node snapshot first so service discovery state stays consistent.
	int removedSnapshotCount = 0;
	for (int i = nodesOfStoppedType.size() - 1; i >= 0; --i)
	{
		const auto &cachedNodeSnapshot = nodesOfStoppedType.Get(i);
		if (!NodeUtils::IsSameNode(cachedNodeSnapshot.node_uuid(), stoppedNode.node_uuid()))
		{
			continue;
		}

		nodesOfStoppedType.DeleteSubrange(i, 1);
		++removedSnapshotCount;
	}

	if (removedSnapshotCount == 0)
	{
		LOG_WARN << "Service node stop did not match local cache. node_id=" << stoppedNode.node_id()
				 << ", node_uuid=" << stoppedNode.node_uuid()
				 << ", node_type=" << stoppedNode.node_type();
	}
	else
	{
		LOG_INFO << "Removed " << removedSnapshotCount << " stale node record(s) for uuid=" << stoppedNode.node_uuid();
	}

	const auto stoppedNodeType = stoppedNode.node_type();
	const auto stoppedNodeId = stoppedNode.node_id();
	const auto stoppedNodeUuid = stoppedNode.node_uuid();
	const auto stoppedProtocolType = stoppedNode.protocol_type();

	// Important: destroy network entities after current channel dispatch cycle.
	// Direct destroy inside etcd watch callback can invalidate activeChannels_.
	GetLoop()->queueInLoop([stoppedNodeType, stoppedNodeId, stoppedNodeUuid, stoppedProtocolType]()
						   {
		entt::registry& nodeRegistry = tlsNodeContextManager.GetRegistry(stoppedNodeType);

		// GRPC nodes use auto-generated entity IDs (not node_id), so look up by uuid.
		if (stoppedProtocolType == PROTOCOL_GRPC) {
			for (const auto& [entity, nodeInfo] : nodeRegistry.view<NodeInfo>().each()) {
				if (!NodeUtils::IsSameNode(nodeInfo.node_uuid(), stoppedNodeUuid)) {
					continue;
				}

				OnNodeRemoveEvent nodeRemovedEvent;
				nodeRemovedEvent.set_entity(entt::to_integral(entity));
				nodeRemovedEvent.set_node_type(stoppedNodeType);
				tlsEcs.dispatcher.trigger(nodeRemovedEvent);

				DestroyEntity(nodeRegistry, entity);
				return;
			}
			LOG_WARN << "GRPC node entity not found for uuid=" << stoppedNodeUuid
				<< ", node_type=" << stoppedNodeType;
			return;
		}

		auto stoppedNodeEntity = entt::entity{ stoppedNodeId };
		if (nodeRegistry.valid(stoppedNodeEntity)) {
			auto* currentNodeInfo = nodeRegistry.try_get<NodeInfo>(stoppedNodeEntity);
			if (currentNodeInfo && !NodeUtils::IsSameNode(currentNodeInfo->node_uuid(), stoppedNodeUuid)) {
				LOG_WARN << "Skip node destroy due to node id reuse. node_id=" << stoppedNodeId
					<< ", stale_uuid=" << stoppedNodeUuid
					<< ", current_uuid=" << currentNodeInfo->node_uuid();
				return;
			}
		}

		OnNodeRemoveEvent nodeRemovedEvent;
		nodeRemovedEvent.set_entity(entt::to_integral(stoppedNodeEntity));
		nodeRemovedEvent.set_node_type(stoppedNodeType);
		tlsEcs.dispatcher.trigger(nodeRemovedEvent);

		DestroyEntity(nodeRegistry, stoppedNodeEntity); });
	LOG_INFO << "Service node stopped : " << stoppedNode.DebugString();
}

void Node::OnServerConnected(const OnConnected2TcpServerEvent &connectedEvent)
{
	eventLoop->assertInLoopThread();
	if (rpcServer == nullptr)
	{
		return;
	}

	auto &connection = connectedEvent.conn_;
	if (!connection->connected())
	{
		LOG_INFO << "Client disconnected: " << connection->peerAddress().toIpPort();
		return;
	}
	LOG_INFO << "Connected to server: " << connection->peerAddress().toIpPort();
	for (uint32_t nodeType = 0; nodeType < eNodeType_ARRAYSIZE; ++nodeType)
	{
		nodeRegistrationManager.TryRegisterNodeSession(nodeType, connection);
	}
}

void Node::StartNodeRegistrationHealthMonitor()
{
	serviceHealthMonitorTimer.RunEvery(tlsNodeConfigManager.GetBaseDeployConfig().health_check_interval(),
									   [this, reRegistrationRequested = false]() mutable
									   {
										   if (rpcServer == nullptr)
										   {
											   return;
										   }

										   // Check lease deadline first — this catches network partitions where
										   // the watch stream is dead and the local ServiceNodeList is stale.
										   if (serviceDiscoveryManager.etcdService.IsLeasePresumablyExpired())
										   {
											   OnNodeIdConflictShutdown(NodeIdConflictReason::kLeaseDeadlineExceeded);
											   LOG_FATAL << "Lease deadline exceeded: no keepalive ACK from etcd within TTL. "
															"node_id="
														 << GetNodeInfo().node_id()
														 << ". Etcd has likely expired our lease; another node may claim this ID. "
															"Terminating to prevent SnowFlake collision. "
															"Active players will reconnect through the normal login flow.";
											   return;
										   }

										   auto &currentNode = GetNodeInfo();

										   auto &serviceNodesByType = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
										   auto &registeredNodesForType = *serviceNodesByType[currentNode.node_type()].mutable_node_list();
										   for (const auto &registeredNode : registeredNodesForType)
										   {
											   if (IsCurrentNode(registeredNode))
											   {
												   reRegistrationRequested = false;
												   return;
											   }
										   }

										   // Node snapshot disappeared from service discovery.
										   // Re-register with the same node_id; do not run full re-allocation flow.
										   if (reRegistrationRequested)
										   {
											   return;
										   }

										   serviceDiscoveryManager.etcdService.RequestReRegistration();
										   reRegistrationRequested = true;
									   });
}
