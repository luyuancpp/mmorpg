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
#include "rpc/service_metadata/centre_service_service_metadata.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "rpc/service_metadata/service_metadata.h"
#include "threading/redis_manager.h"
#include "time/system/time.h"
#include "network/network_utils.h"
#include "proto_helpers/proto_util.h"
#include "core/utils/debug/stacktrace_system.h"
#include "network/node_utils.h"
#include <boost/algorithm/string.hpp>
#include "node/system/etcd/etcd_service.h"
#include "node/system/node/node_connector.h"
#include "node/system/node/node_allocator.h"
#include "threading/node_context_manager.h"
#include <node_config_manager.h>
#include <threading/registry_manager.h>
#include "threading/thread_local_entity_container.h"
#include <threading/entity_manager.h>
#include <cstdio>
#include <atomic>
#include <future>
#include <chrono>

namespace {
std::atomic<Node*> gNodeAtomic{ nullptr };

void StdoutOutput(const char* msg, int len) {
	std::fwrite(msg, 1, static_cast<size_t>(len), stdout);
}
}

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

Node* gNode;

Node::Node(muduo::net::EventLoop* loop, const std::string& logPath)
	: eventLoop(loop), logSystem(logPath, kMaxLogFileRollSize, 1) {
	if (eventLoop == nullptr) {
		LOG_FATAL << "Node requires a valid EventLoop pointer.";
	}

	LOG_INFO << "Node created, log file: " << logPath;
	if (gNode != nullptr && gNode != this) {
		LOG_FATAL << "Multiple Node instances detected. existing=" << gNode << ", new=" << this;
	}

	gNode = this;
	gNodeAtomic.store(this, std::memory_order_release);
	tlsRegistryManager.nodeGlobalRegistry.emplace<ServiceNodeList>(GetGlobalGrpcNodeEntity());

	//未实现的节点实现一个空函数
	void InitPlayerService();
	InitPlayerService();

	void InitPlayerServiceReplied();
	InitPlayerServiceReplied();

	void InitServiceHandler();
	InitServiceHandler();
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
	return tlsRegistryManager.globalRegistry.get_or_emplace<NodeInfo>(GlobalEntity());
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
	NodeInfo& info = GetNodeInfo();
	info.mutable_endpoint()->set_ip(localip());
	info.mutable_endpoint()->set_port(get_available_port(GetNodeType() * 10000));
	info.set_node_type(GetNodeType());
	info.set_scene_node_type(tlsNodeConfigManager.GetGameConfig().scene_node_type());
	info.set_protocol_type(PROTOCOL_TCP);
	info.set_launch_time(TimeSystem::NowMicrosecondsUTC());
	info.set_zone_id(tlsNodeConfigManager.GetGameConfig().zone_id());

	info.set_node_uuid(boost::uuids::to_string(gen()));

	InetAddress addr(tlsNodeConfigManager.GetGameConfig().zone_redis().host(), tlsNodeConfigManager.GetGameConfig().zone_redis().port());
	tlsReids.GetZoneRedis() = std::make_unique<RedisManager::HiredisPtr::element_type>(eventLoop, addr);
	tlsReids.GetZoneRedis()->connect();

	LOG_DEBUG << "Node info: " << info.DebugString();
}

void Node::InitKafka()
{
	kafkaManager.Init(tlsNodeConfigManager.GetBaseDeployConfig().kafka());
}

void Node::StartKafkaPolling()
{
	kafkaConsumerTimer.RunEvery(0.1, [this] { kafkaManager.Poll(); });
}

void Node::InitEtcdService()
{
	serviceDiscoveryManager.Init();
}

void Node::StartRpcServer() {
	eventLoop->assertInLoopThread();
	if (rpcServer) {
		LOG_TRACE << "RPC server already started, skipping.";
		return;
	}

	NodeInfo& info = GetNodeInfo();
	InetAddress addr(info.endpoint().ip(), info.endpoint().port());

	rpcServer = std::make_unique<RpcServerPtr::element_type>(eventLoop, addr);
	rpcServer->start();
	auto* replyService = GetNodeReplyService();
	if (replyService != nullptr) {
		rpcServer->registerService(replyService);
	}
	else {
		LOG_WARN << "Node reply service is null, skip registerService for node_type=" << GetNodeInfo().node_type();
	}

	for (auto it = gNodeService.begin(); it != gNodeService.end(); ++it) {
		auto& serviceName = it->first;
		auto& service = it->second;
		if (service == nullptr) {
			LOG_WARN << "Skip null node service registration for key=" << serviceName;
			continue;
		}

		rpcServer->registerService(service.get());
	}

	NodeConnector::ConnectAllNodes();

	StartServiceHealthMonitor();

	dispatcher.trigger<OnServerStart>();

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

	std::promise<void> shutdownDone;
	auto shutdownFuture = shutdownDone.get_future();
	eventLoop->runInLoop([this, &shutdownDone]() {
		ShutdownInLoop();
		shutdownDone.set_value();
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
	tlsThreadLocalEntityContainer.Clear();
	tlsRegistryManager.Clear();
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
	dispatcher.sink<OnConnected2TcpServerEvent>().connect<&Node::OnServerConnected>(*this);
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

void InitReply();
void Node::RegisterHandlers() {
	InitMessageInfo();
	InitReply();
}

void Node::AsyncOutput(const char* msg, int len) {
	Node* node = gNodeAtomic.load(std::memory_order_acquire);
	if (node != nullptr) {
		node->Log().append(msg, len);
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

void Node::CallRemoteMethodZoneCenter(uint32_t message_id, const ::google::protobuf::Message& request)
{
	if (nullptr == GetZoneCentreNode()){
		return;
	}
	GetZoneCentreNode()->CallRemoteMethod(message_id, request);
}

bool Node::IsMyNode(const NodeInfo& node) const
{
	return NodeUtils::IsSameNode(node.node_uuid(), GetNodeInfo().node_uuid());
}

void Node::HandleServiceNodeStop(const std::string& key, const std::string& nodeJson) {
	eventLoop->assertInLoopThread();
	LOG_INFO << "Service node stop, key: " << key << ", value: " << nodeJson;

	NodeInfo deleteNode;
	auto parseResult = google::protobuf::util::JsonStringToMessage(nodeJson, &deleteNode);
	if (!parseResult.ok()) {
		LOG_ERROR << "Parse node JSON failed, key: " << key
			<< ", JSON: " << nodeJson
			<< ", Error: " << parseResult.message().data();
		return;
	}

	if (!eNodeType_IsValid(deleteNode.node_type())) {
		LOG_TRACE << "Unknown service type for key: " << key;
		return;
	}

	if (deleteNode.node_uuid().empty()) {
		LOG_WARN << "Ignore service node stop with empty node_uuid. key=" << key;
		return;
	}

	auto& nodeRegistry = tlsRegistryManager.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& nodeList = *nodeRegistry[deleteNode.node_type()].mutable_node_list();

	// Remove stale node snapshot first so service discovery state stays consistent.
	int removedCount = 0;
	for (int i = nodeList.size() - 1; i >= 0; --i) {
		const auto& node = nodeList.Get(i);
		if (!NodeUtils::IsSameNode(node.node_uuid(), deleteNode.node_uuid())) {
			continue;
		}

		nodeList.DeleteSubrange(i, 1);
		++removedCount;
	}

	if (removedCount == 0) {
		LOG_WARN << "Service node stop did not match local cache. node_id=" << deleteNode.node_id()
			<< ", node_uuid=" << deleteNode.node_uuid()
			<< ", node_type=" << deleteNode.node_type();
	}
	else {
		LOG_INFO << "Removed " << removedCount << " stale node record(s) for uuid=" << deleteNode.node_uuid();
	}

	const auto nodeType = deleteNode.node_type();
	const auto nodeId = deleteNode.node_id();
	const auto nodeUuid = deleteNode.node_uuid();

	// Important: destroy network entities after current channel dispatch cycle.
	// Direct destroy inside etcd watch callback can invalidate activeChannels_.
	GetLoop()->queueInLoop([nodeType, nodeId, nodeUuid]() {
		auto nodeEntity = entt::entity{ nodeId };
		entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);

		if (registry.valid(nodeEntity)) {
			auto* currentNodeInfo = registry.try_get<NodeInfo>(nodeEntity);
			if (currentNodeInfo && !NodeUtils::IsSameNode(currentNodeInfo->node_uuid(), nodeUuid)) {
				LOG_WARN << "Skip node destroy due to node id reuse. node_id=" << nodeId
					<< ", stale_uuid=" << nodeUuid
					<< ", current_uuid=" << currentNodeInfo->node_uuid();
				return;
			}
		}

		OnNodeRemovePbEvent onNodeRemovePbEvent;
		onNodeRemovePbEvent.set_entity(entt::to_integral(nodeEntity));
		onNodeRemovePbEvent.set_node_type(nodeType);
		dispatcher.trigger(onNodeRemovePbEvent);

		DestroyEntity(registry, nodeEntity);
	});
	LOG_INFO << "Service node stopped : " << deleteNode.DebugString();
}

void Node::OnServerConnected(const OnConnected2TcpServerEvent& event) {
	eventLoop->assertInLoopThread();
	if (rpcServer == nullptr) {
		return;
	}

	auto& conn = event.conn_;
	if (!conn->connected()) {
		LOG_INFO << "Client disconnected: " << conn->peerAddress().toIpPort();
		return;
	}
	LOG_INFO << "Connected to server: " << conn->peerAddress().toIpPort();
	for (uint32_t i = 0; i < eNodeType_ARRAYSIZE; ++i)
	{
		nodeRegistrationManager.TryRegisterNodeSession(i, conn);
	}
}

void Node::StartServiceHealthMonitor(){
	serviceHealthMonitorTimer.RunEvery(tlsNodeConfigManager.GetBaseDeployConfig().health_check_interval(), [this]() {
		if (nullptr == rpcServer)
		{
			return;
		}
		auto& myNode = GetNodeInfo();

		auto& nodeRegistry = tlsRegistryManager.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(GetGlobalGrpcNodeEntity());
		auto& nodeList = *nodeRegistry[myNode.node_type()].mutable_node_list();
		for (auto it = nodeList.begin(); it != nodeList.end(); ++it) {
			if (IsMyNode(*it)) {
				return ;
			}
		}

		etcdManager.RequestEtcdLease();
		}
	);
}


