#include "node.h"
#include <ranges>
#include <regex>
#include <grpcpp/create_channel.h>
#include <boost/uuid/uuid_io.hpp>
#include "all_config.h"
#include "etcd_helper.h"
#include "config_loader/config.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/message_differencer.h"
#include "grpc/generator/grpc_init.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "log/constants/log_constants.h"
#include "log/system/console_log_system.h"
#include "logic/event/server_event.pb.h"
#include "muduo/base/TimeZone.h"
#include "network/process_info.h"
#include "network/rpc_session.h"
#include "node/system/node_util.h"
#include "pbc/common_error_tip.pb.h"
#include "proto/common/node.pb.h"
#include "proto/logic/event/node_event.pb.h"
#include "service_info/centre_service_service_info.h"
#include "service_info/game_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "service_info/service_info.h"
#include "thread_local/storage_common_logic.h"
#include "time/system/time_system.h"
#include "util/network_utils.h"
#include "util/random.h"
#include "generator/util/gen_util.h"
#include "util/stacktrace_system.h"
#include "util/node_utils.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <fmt/base.h>
#include "etcd_manager.h"
#include "node_connector.h"
#include "node_allocator.h"

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

Node* gNode;

Node::Node(muduo::net::EventLoop* loop, const std::string& logPath)
	: eventLoop(loop), logSystem(logPath, kMaxLogFileRollSize, 1) {
	LOG_INFO << "Node created, log file: " << logPath;

	gNode = this;
	tls.nodeGlobalRegistry.emplace<ServiceNodeList>(GetGlobalGrpcNodeEntity());

	//未实现的节点实现一个空函数
	void InitPlayerService();
	InitPlayerService();

	void InitPlayerServiceReplied();
	InitPlayerServiceReplied();

	void InitServiceHandler();
	InitServiceHandler();

	Initialize();
}

Node::~Node() {
	Shutdown();
}

NodeInfo& Node::GetNodeInfo() const {
	return tls.globalRegistry.get_or_emplace<NodeInfo>(GlobalEntity());
}

void Node::Initialize() {
	LOG_DEBUG << "Node initializing...";
    SetupTimeZone();
	RegisterHandlers();
	RegisterEventHandlers();
	LoadConfigs();
	InitRpcServer();
	InitLogSystem();
	LoadAllConfigData();
	InitKafka();
	InitGrpcClients();
	FetchServiceNodes();
	LOG_DEBUG << "Node initialization complete.";
}

void Node::InitRpcServer() {
	NodeInfo& info = GetNodeInfo();
	info.mutable_endpoint()->set_ip(localip());
	info.mutable_endpoint()->set_port(get_available_port(GetNodeType() * 10000));
	info.set_node_type(GetNodeType());
	info.set_scene_node_type(tlsCommonLogic.GetGameConfig().scene_node_type());
	info.set_protocol_type(PROTOCOL_TCP);
	info.set_launch_time(TimeUtil::NowMicrosecondsUTC());
	info.set_zone_id(tlsCommonLogic.GetGameConfig().zone_id());

	info.set_node_uuid(boost::uuids::to_string(gen()));

	InetAddress addr(tlsCommonLogic.GetGameConfig().zone_redis().host(), tlsCommonLogic.GetGameConfig().zone_redis().port());
	tlsCommonLogic.GetZoneRedis() = std::make_unique<ThreadLocalStorageCommonLogic::HiredisPtr::element_type>(eventLoop, addr);
	tlsCommonLogic.GetZoneRedis()->connect();

	LOG_DEBUG << "Node info: " << info.DebugString();
}

void Node::InitKafka()
{
	kafkaManager.Init(tlsCommonLogic.GetBaseDeployConfig().kafka());
}

void Node::StartRpcServer() {
	if (rpcServer) {
		LOG_TRACE << "RPC server already started, skipping.";
		return;
	}

	NodeInfo& info = GetNodeInfo();
	InetAddress addr(info.endpoint().ip(), info.endpoint().port());

	rpcServer = std::make_unique<RpcServerPtr::element_type>(eventLoop, addr);
	rpcServer->start();
	rpcServer->registerService(GetNodeReplyService());

	for (auto& val : gNodeService | std::views::values) {
		rpcServer->registerService(val.get());
	}

	NodeConnector::ConnectAllNodes();

	StartServiceHealthMonitor();

	tls.dispatcher.trigger<OnServerStart>();

	auto nodeTypeName = boost::to_upper_copy(eNodeType_Name(GetNodeInfo().node_type()));
	LOG_INFO << "\n\n"
		<< "=============================================================\n"
		<< "	" << nodeTypeName << " NODE STARTED SUCCESSFULLY\n"
		<< "	Node Info:\n" << GetNodeInfo().DebugString() << "\n"
		<< "=============================================================\n";
}

void Node::Shutdown() {
	LOG_DEBUG << "Node shutting down...";
	StopWatchingServiceNodes();
	tls.Clear();
	logSystem.stop();
	ReleaseNodeId();
	EtcdManager::Shutdown();
	grpcHandlerTimer.Cancel();
	LOG_DEBUG << "Node shutdown complete.";
}

void Node::InitLogSystem() {
	auto logLevel = static_cast<muduo::Logger::LogLevel>(
		tlsCommonLogic.GetBaseDeployConfig().log_level()
		);
	muduo::Logger::setLogLevel(logLevel);
	muduo::Logger::setOutput(AsyncOutput);
	logSystem.start();
}

void Node::RegisterEventHandlers() {
	tls.dispatcher.sink<OnConnected2TcpServerEvent>().connect<&Node::OnServerConnected>(*this);
	tls.dispatcher.sink<OnTcpClientConnectedEvent>().connect<&Node::OnClientConnected>(*this);
}

void Node::LoadConfigs() {
	readBaseDeployConfig("etc/base_deploy_config.yaml", tlsCommonLogic.GetBaseDeployConfig());
	readGameConfig("etc/game_config.yaml", tlsCommonLogic.GetGameConfig());
}

void Node::LoadAllConfigData() {
	LoadConfigsAsync();
}

void Node::SetupTimeZone() {
	const muduo::TimeZone hkTz("zoneinfo/Asia/Hong_Kong");
	muduo::Logger::setTimeZone(hkTz);
}

void Node::InitGrpcClients() {
	const std::string& etcdAddr = *tlsCommonLogic.GetBaseDeployConfig().etcd_hosts().begin();
	auto channel = grpc::CreateChannel(etcdAddr, grpc::InsecureChannelCredentials());
	InitGrpcNode(channel, tls.GetNodeRegistry(EtcdNodeService), tls.GetNodeGlobalEntity(EtcdNodeService));

	LOG_INFO << "Initializing gRPC client to etcd address: " << etcdAddr;

	grpcHandlerTimer.RunEvery(0.005, [] {
		for (auto&registry : tls.GetNodeRegistry()){
			HandleCompletedQueueMessage(registry);
		}
		});
}

void Node::FetchServiceNodes() {
	for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
		EtcdHelper::RangeQuery(prefix);
	}
}

void Node::StopWatchingServiceNodes() {
	EtcdHelper::StopAllWatching();
}

void Node::ReleaseNodeId() {
	EtcdHelper::RevokeLeaseAndCleanup(leaseId);
}

void InitRepliedHandler();
void Node::RegisterHandlers() {
	InitMessageInfo();
	InitGrpcResponseHandlers();
	InitRepliedHandler();
}

void Node::AsyncOutput(const char* msg, int len) {
	gNode->Log().append(msg, len);
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

void Node::AddServiceNode(const std::string& nodeJson, uint32_t nodeType) {
	LOG_INFO << "Add service node type " << nodeType << " JSON: " << nodeJson;

	if (!eNodeType_IsValid(static_cast<int32_t>(nodeType))) {
		LOG_ERROR << "Invalid node type: " << nodeType;
		return;
	}

	NodeInfo newNode;
	auto parseResult = google::protobuf::util::JsonStringToMessage(nodeJson, &newNode);
	if (!parseResult.ok()) {
		LOG_ERROR << "Parse node JSON failed, type: " << nodeType
			<< ", JSON: " << nodeJson
			<< ", Error: " << parseResult.message().data();
		return;
	}

	auto& nodeRegistry = tls.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& nodeList = *nodeRegistry[nodeType].mutable_node_list();

	*nodeList.Add() = newNode;
	LOG_INFO << "Node added, type: " << nodeType << ", info: " << newNode.DebugString();

	if (IsMyNode(newNode)) {
		LOG_TRACE << "Node has same lease_id as self, skip adding node. Self uuid: " << newNode.node_uuid();
		return;
	}

	if (!targetNodeTypeWhitelist.contains(nodeType)) return;

	if (IsServiceStarted()) {
		NodeConnector::ConnectToNode(newNode);
		LOG_INFO << "Connected to node: " << newNode.DebugString();
	}
	else {
		LOG_INFO << "Service not started or node already connected. Skipping connection for now: " << newNode.DebugString();
	}
}

bool Node::IsMyNode(const NodeInfo& node) const
{
	return NodeUtils::IsSameNode(node.node_uuid(), GetNodeInfo().node_uuid());
}

void Node::HandleServiceNodeStart(const std::string& key, const std::string& value) {
	LOG_TRACE << "Service node start, key: " << key << ", value: " << value;
	if (const auto nodeType = NodeUtils::GetServiceTypeFromPrefix(key); eNodeType_IsValid(nodeType)) {
		AddServiceNode(value, nodeType);
	}
}

void Node::HandleServiceNodeStop(const std::string& key, const std::string& nodeJson) {
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

	// 下面是你的节点处理代码
	auto& nodeRegistry = tls.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& nodeList = *nodeRegistry[deleteNode.node_type()].mutable_node_list();
	if (deleteNode.protocol_type() == PROTOCOL_GRPC)
	{
		auto nodeEntity = entt::entity{ deleteNode.node_id() };
		entt::registry& registry = tls.GetNodeRegistry(deleteNode.node_type());

		OnNodeRemovePbEvent onNodeRemovePbEvent;
		onNodeRemovePbEvent.set_entity(entt::to_integral(nodeEntity));
		onNodeRemovePbEvent.set_node_type(deleteNode.node_type());
		tls.dispatcher.trigger(onNodeRemovePbEvent);

		Destroy(registry, nodeEntity);
	}
	LOG_INFO << "Service node stopped : " << deleteNode.DebugString();
}

void Node::InitGrpcResponseHandlers() {
	etcdserverpb::AsyncKVRangeHandler = [this](const ClientContext& context, const ::etcdserverpb::RangeResponse& reply) {
		std::unordered_set<std::string> seenPrefixes;

		// 初始化所有前缀的下次 revision（默认都设为 reply 的 revision + 1）
		int64_t nextRevision = reply.header().revision() + 1;

		// 你维护了每个 prefix 是否在本次 Range 中出现
		std::unordered_map<std::string, bool> prefixSeen;

		// 初始化为 false
		for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
			prefixSeen[prefix] = false;
		}

		// 处理所有 kv
		for (const auto& kv : reply.kvs()) {
			HandleServiceNodeStart(kv.key(), kv.value());

			for (const auto& prefix : prefixSeen) {
				if (kv.key().rfind(prefix.first, 0) == 0) { // prefix match
					prefixSeen[prefix.first] = true;
					break;
				}
			}
		}

		// 设置 revision
		for (const auto& [prefix, seen] : prefixSeen) {
			revision[prefix] = nextRevision;
		}

		// 启动 watch
		if (!hasSentRange) {
			for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
				EtcdHelper::StartWatchingPrefix(prefix, revision[prefix]);
				LOG_INFO << "Start watching prefix: " << prefix << " from revision " << revision[prefix];
			}
			hasSentRange = true;
		}
		};

	etcdserverpb::AsyncKVPutHandler = [this](const ClientContext& context, const ::etcdserverpb::PutResponse& reply) {
		LOG_INFO << "Put response: " << reply.DebugString();
		};

	etcdserverpb::AsyncKVDeleteRangeHandler = [](const ClientContext& context, const ::etcdserverpb::DeleteRangeResponse& reply) {};

	etcdserverpb::AsyncKVTxnHandler = [this](const ClientContext& context, const ::etcdserverpb::TxnResponse& reply) {
		LOG_INFO << "Txn response: " << reply.DebugString();

		auto& key = pendingKeys.front();

		if (reply.succeeded()) {
			if (boost::algorithm::starts_with(key, EtcdManager::MakeNodePortEtcdPrefix(GetNodeInfo()))) {
				StartRpcServer();
			}
			else if (boost::algorithm::starts_with(key, EtcdManager::MakeNodeEtcdPrefix(GetNodeInfo()))) {
				tls.OnNodeStart(GetNodeInfo().node_id());
			}
		}
		else {
			if (boost::algorithm::starts_with(key, EtcdManager::MakeNodeEtcdPrefix(GetNodeInfo()))) {
				// 只有 node key 失败才尝试重新 AcquireNode
				acquireNodeTimer.RunAfter(1, [this]() { NodeAllocator::AcquireNode(); });
			}
			else {
				acquirePortTimer.RunAfter(1, [this]() { NodeAllocator::AcquireNodePort(); });
			}
		}

		pendingKeys.pop_front();
		};


	etcdserverpb::AsyncWatchWatchHandler = [this](const ClientContext& context, const ::etcdserverpb::WatchResponse& response) {
		if (!hasSentWatch)
		{
			EtcdManager::RequestEtcdLease();
			hasSentWatch = true;
		}
		if (response.created()) {
			LOG_TRACE << "Watch created.";
			return;
		}
		if (response.canceled()) {
			LOG_INFO << "Watch canceled: " << response.cancel_reason();
			if (response.compact_revision() > 0) {
				LOG_ERROR << "Revision compacted: " << response.compact_revision();
			}
			return;
		}
		for (const auto& event : response.events()) {
			if (event.type() == mvccpb::Event_EventType::Event_EventType_PUT) {
				LOG_INFO << "Key put: " << event.kv().key();
				HandleServiceNodeStart(event.kv().key(), event.kv().value());
			}
			else if (event.type() == mvccpb::Event_EventType::Event_EventType_DELETE) {
				HandleServiceNodeStop(event.kv().key(), event.prev_kv().value());
				LOG_INFO << "Key deleted: " << event.kv().key();
			}
		}
		};

	etcdserverpb::AsyncLeaseLeaseGrantHandler = [this](const ClientContext& context, const ::etcdserverpb::LeaseGrantResponse& reply) {
		// 如果原来没有租约，说明是第一次获取，需要初始化节点信息
		if (leaseId <= 0) {
			LOG_INFO << "Acquiring new lease, ID: " << reply.id();
			leaseId = reply.id();
			NodeAllocator::AcquireNodePort();
			NodeAllocator::AcquireNode();  // 获取节点ID或其他信息
			EtcdManager::KeepNodeAlive();
		}
		else {
			LOG_INFO << "Lease already exists, updating lease_id: " << reply.id();
			// 租约过期后重新获取，需要重新注册服务节点
			leaseId = reply.id();
			EtcdManager::KeepNodeAlive();
			EtcdManager::RegisterNodeService();
		}

		LOG_INFO << "Lease granted: " << reply.DebugString();
		};

	auto emptEtcdHandler = [](const ClientContext& context, const ::google::protobuf::Message& reply) {};
	if (!etcdserverpb::AsyncKVCompactHandler) {
		etcdserverpb::AsyncKVCompactHandler = emptEtcdHandler;
	}
	if (!etcdserverpb::AsyncLeaseLeaseRevokeHandler) {
		etcdserverpb::AsyncLeaseLeaseRevokeHandler = emptEtcdHandler;
	}
	if (!etcdserverpb::AsyncLeaseLeaseKeepAliveHandler) {
		etcdserverpb::AsyncLeaseLeaseKeepAliveHandler = emptEtcdHandler;
	}
	if (!etcdserverpb::AsyncLeaseLeaseTimeToLiveHandler) {
		etcdserverpb::AsyncLeaseLeaseTimeToLiveHandler = emptEtcdHandler;
	}
	if (!etcdserverpb::AsyncLeaseLeaseLeasesHandler) {
		etcdserverpb::AsyncLeaseLeaseLeasesHandler = emptEtcdHandler;
	}
}

void Node::OnServerConnected(const OnConnected2TcpServerEvent& event) {
	auto& conn = event.conn_;
	if (!conn->connected()) {
		LOG_INFO << "Client disconnected: " << conn->peerAddress().toIpPort();
		return;
	}
	LOG_INFO << "Connected to server: " << conn->peerAddress().toIpPort();
	for (uint32_t i = 0; i < eNodeType_ARRAYSIZE; ++i)
	{
		TryRegisterNodeSession(i, conn);
	}
}

static uint32_t kNodeTypeToMessageId[eNodeType_ARRAYSIZE] = {
	0,
	0,
	CentreRegisterNodeSessionMessageId,
	SceneRegisterNodeSessionMessageId,
	GateRegisterNodeSessionMessageId
};

void Node::TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const {
	entt::registry& registry = tls.GetNodeRegistry(nodeType);
	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
		if (!IsSameAddress(client->peer_addr(), conn->peerAddress())) continue;
		LOG_INFO << "Peer address match in " << NodeUtils::GetRegistryName(registry)
			<< ": " << conn->peerAddress().toIpPort();
		registry.emplace<TimerTaskComp>(entity).RunAfter(0.5, [conn, this, nodeType, &client]() {
			RegisterNodeSessionRequest req;
			req.mutable_self_node()->CopyFrom(GetNodeInfo());
			req.mutable_endpoint()->set_ip(conn->localAddress().toIp());
			req.mutable_endpoint()->set_port(conn->localAddress().port());
			client->CallRemoteMethod(kNodeTypeToMessageId[nodeType], req);
			});
		return;
	}
}

void Node::OnClientConnected(const OnTcpClientConnectedEvent& event) {
	auto& conn = event.conn_;
	if (!conn->connected()) {
		for (const auto& [entity, session] : tls.sessionRegistry.view<RpcSession>().each()) {
			auto& existConn = session.connection;
			if (!IsSameAddress(conn->peerAddress(), existConn->peerAddress())) {
				LOG_TRACE << "Endpoint mismatch: expected " << conn->peerAddress().toIp()
					<< ":" << conn->peerAddress().port()
					<< ", actual " << existConn->peerAddress().toIp()
					<< ":" << existConn->peerAddress().port();
				continue;
			}
			tls.sessionRegistry.destroy(entity);
			return;
		}
		return;
	}
	auto entity = tls.sessionRegistry.create();
	tls.sessionRegistry.emplace<RpcSession>(entity, RpcSession{ conn });
	LOG_INFO << "Client connected: " << conn->peerAddress().toIpPort();
}

void Node::HandleNodeRegistration(
	const RegisterNodeSessionRequest& request,
	RegisterNodeSessionResponse& response
) const {
	auto& peerNode = request.self_node();
	response.mutable_peer_node()->CopyFrom(GetNodeInfo());
	LOG_TRACE << "Node registration request: " << request.DebugString();

	auto tryRegister = [&, this](const TcpConnectionPtr& conn, uint32_t nodeType) -> bool {
		entt::registry& registry = tls.GetNodeRegistry(nodeType);
		const auto& nodeList = tls.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
		for (auto& serverNode : nodeList[nodeType].node_list()) {
			if (!NodeUtils::IsSameNode(serverNode.node_uuid(), peerNode.node_uuid())) continue;
			entt::entity nodeEntity = entt::entity{ serverNode.node_id() };
			entt::entity created = ResetEntity(registry, nodeEntity);
			if (created == entt::null) {
				LOG_ERROR << "Create node entity failed in " << NodeUtils::GetRegistryName(registry);
				return false;
			}
			registry.emplace<RpcSession>(created, RpcSession{ conn });
			LOG_INFO << "Node registered, id: " << peerNode.node_id()
				<< " in " << NodeUtils::GetRegistryName(registry);
			return true;
		}
		return false;
		};

	for (const auto& [entity, session] : tls.sessionRegistry.view<RpcSession>().each()) {
		auto& conn = session.connection;
		if (!IsSameAddress(conn->peerAddress(), muduo::net::InetAddress(request.endpoint().ip(), request.endpoint().port()))) continue;
		for (uint32_t nodeType = eNodeType_MIN; nodeType < eNodeType_ARRAYSIZE; ++nodeType) {
			if (peerNode.node_type() != nodeType || !IsTcpNodeType(nodeType)) continue;
			if (tryRegister(conn, nodeType)) {
				tls.sessionRegistry.destroy(entity);
				response.mutable_error_message()->set_id(kCommon_errorOK);
				LOG_INFO << "Node registration succeeded: " << peerNode.DebugString();
				return;
			}
		}
	}
	response.mutable_error_message()->set_id(kFailedToRegisterTheNode);
}

void TriggerNodeConnectionEvent(entt::registry& registry, const RegisterNodeSessionResponse& response) {
	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
		if (client->peer_addr().toIp() != response.peer_node().endpoint().ip() ||
			client->peer_addr().port() != response.peer_node().endpoint().port()) {
			continue;
		}
		ConnectToNodePbEvent connectEvent;
		connectEvent.set_entity(entt::to_integral(entity));
		connectEvent.set_node_type(nodeInfo.node_type());
		tls.dispatcher.trigger(connectEvent);
		if (nodeInfo.node_type() == CentreNodeService) {
			OnConnect2CentrePbEvent centreEvent;
			centreEvent.set_entity(entt::to_integral(entity));
			tls.dispatcher.trigger(centreEvent);
			LOG_INFO << "CentreNode connected, entity: " << entt::to_integral(entity);
		}
		registry.remove<TimerTaskComp>(entity);
		break;
	}
}

void Node::HandleNodeRegistrationResponse(const RegisterNodeSessionResponse& response) const {
	LOG_INFO << "Node registration response: " << response.DebugString();
	uint32_t nodeType = response.peer_node().node_type();
	entt::registry& registry = tls.GetNodeRegistry(nodeType);
	if (response.error_message().id() != kCommon_errorOK) {
		LOG_TRACE << "Registration failed: " << response.DebugString();
		for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
			if (!IsSameAddress(client->peer_addr(), muduo::net::InetAddress(
				response.peer_node().endpoint().ip(),
				response.peer_node().endpoint().port()))) continue;
			registry.get<TimerTaskComp>(entity).RunAfter(0.5, [this, &client, nodeType]() {
				RegisterNodeSessionRequest req;
				*req.mutable_self_node() = GetNodeInfo();
				req.mutable_endpoint()->set_ip(client->local_addr().toIp());
				req.mutable_endpoint()->set_port(client->local_addr().port());
				client->CallRemoteMethod(kNodeTypeToMessageId[nodeType], req);
				});
			return;
		}
		return;
	}
	entt::entity peerEntity{ response.peer_node().node_id() };
	registry.remove<TimerTaskComp>(peerEntity);
	TriggerNodeConnectionEvent(registry, response);
	LOG_INFO << "Node registration success.";
}

void Node::StartServiceHealthMonitor(){
	serviceHealthMonitorTimer.RunEvery(tlsCommonLogic.GetBaseDeployConfig().health_check_interval(), [this]() {
		if (nullptr == rpcServer)
		{
			return;
		}
		auto& myNode = GetNodeInfo();

		auto& nodeRegistry = tls.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
		auto& nodeList = *nodeRegistry[myNode.node_type()].mutable_node_list();
		for (auto it = nodeList.begin(); it != nodeList.end(); ++it) {
			if (IsMyNode(*it)) {
				return ;
			}
		}

		EtcdManager::RequestEtcdLease();
		}
	);
}


