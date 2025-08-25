#include "node.h"
#include <ranges>
#include <regex>
#include <grpcpp/create_channel.h>
#include <boost/uuid/uuid_io.hpp>
#include "config/all_config.h"
#include "etcd_helper.h"
#include "config.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/message_differencer.h"
#include "grpc/generator/grpc_init.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "log/constants/log_constants.h"
#include "log/system/console_log_system.h"
#include "proto/logic/event/server_event.pb.h"
#include "muduo/base/TimeZone.h"
#include "network/process_info.h"
#include "network/rpc_session.h"
#include "node/system/node_util.h"
#include "proto/config/common_error_tip.pb.h"
#include "proto/common/node.pb.h"
#include "proto/logic/event/node_event.pb.h"
#include "service_info/centre_service_service_info.h"
#include "service_info/game_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "service_info/service_info.h"
#include "threading/redis_manager.h"
#include "time/system/time_system.h"
#include "network/network_utils.h"
#include "util/gen_util.h"
#include "util/stacktrace_system.h"
#include "network/node_utils.h"
#include <boost/algorithm/string.hpp>
#include "etcd_service.h"
#include "node_connector.h"
#include "node_allocator.h"
#include "threading/node_context_manager.h"
#include <node_config_manager.h>
#include <threading/registry_manager.h>
#include "threading/thread_local_entity_container.h"
#include <threading/entity_manager.h>

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

Node* gNode;

Node::Node(muduo::net::EventLoop* loop, const std::string& logPath)
	: eventLoop(loop), logSystem(logPath, kMaxLogFileRollSize, 1) {
	LOG_INFO << "Node created, log file: " << logPath;

	gNode = this;
	tlsRegistryManager.nodeGlobalRegistry.emplace<ServiceNodeList>(GetGlobalGrpcNodeEntity());

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
	serviceDiscoveryManager.Shutdown();
	Shutdown();
}

NodeInfo& Node::GetNodeInfo() const {
	return tlsRegistryManager.globalRegistry.get_or_emplace<NodeInfo>(GlobalEntity());
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
	info.set_launch_time(TimeUtil::NowMicrosecondsUTC());
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

void Node::InitEtcdService()
{
	serviceDiscoveryManager.Init();
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

	dispatcher.trigger<OnServerStart>();

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
	tlsThreadLocalEntityContainer.Clear();
	tlsRegistryManager.Clear();
	logSystem.stop();
	ReleaseNodeId();
	gNode->GetEtcdManager().Shutdown();
	grpcHandlerTimer.Cancel();
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
	dispatcher.sink<OnTcpClientConnectedEvent>().connect<&Node::OnClientConnected>(*this);
}

void Node::LoadConfigs() {
	readBaseDeployConfig("etc/base_deploy_config.yaml", tlsNodeConfigManager.GetBaseDeployConfig());
	readGameConfig("etc/game_config.yaml", tlsNodeConfigManager.GetGameConfig());
}

void Node::LoadAllConfigData() {
	LoadConfigsAsync();
}

void Node::SetupTimeZone() {
	const muduo::TimeZone hkTz("zoneinfo/Asia/Hong_Kong");
	muduo::Logger::setTimeZone(hkTz);
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

bool Node::IsMyNode(const NodeInfo& node) const
{
	return NodeUtils::IsSameNode(node.node_uuid(), GetNodeInfo().node_uuid());
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
	auto& nodeRegistry = tlsRegistryManager.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& nodeList = *nodeRegistry[deleteNode.node_type()].mutable_node_list();
	if (deleteNode.protocol_type() == PROTOCOL_GRPC)
	{
		auto nodeEntity = entt::entity{ deleteNode.node_id() };
		entt::registry& registry = tlsNodeContextManager.GetRegistry(deleteNode.node_type());

		OnNodeRemovePbEvent onNodeRemovePbEvent;
		onNodeRemovePbEvent.set_entity(entt::to_integral(nodeEntity));
		onNodeRemovePbEvent.set_node_type(deleteNode.node_type());
		dispatcher.trigger(onNodeRemovePbEvent);

		Destroy(registry, nodeEntity);
	}
	LOG_INFO << "Service node stopped : " << deleteNode.DebugString();
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
		nodeRegistrationManager.TryRegisterNodeSession(i, conn);
	}
}

void Node::OnClientConnected(const OnTcpClientConnectedEvent& event) {
	auto& conn = event.conn_;
	if (!conn->connected()) {
		for (const auto& [entity, session] : tlsRegistryManager.sessionRegistry.view<RpcSession>().each()) {
			auto& existConn = session.connection;
			if (!IsSameAddress(conn->peerAddress(), existConn->peerAddress())) {
				LOG_TRACE << "Endpoint mismatch: expected " << conn->peerAddress().toIp()
					<< ":" << conn->peerAddress().port()
					<< ", actual " << existConn->peerAddress().toIp()
					<< ":" << existConn->peerAddress().port();
				continue;
			}
			tlsRegistryManager.sessionRegistry.destroy(entity);
			return;
		}
		return;
	}
	auto entity = tlsRegistryManager.sessionRegistry.create();
	tlsRegistryManager.sessionRegistry.emplace<RpcSession>(entity, RpcSession{ conn });
	LOG_INFO << "Client connected: " << conn->peerAddress().toIpPort();
}

void Node::StartServiceHealthMonitor(){
	serviceHealthMonitorTimer.RunEvery(tlsNodeConfigManager.GetBaseDeployConfig().health_check_interval(), [this]() {
		if (nullptr == rpcServer)
		{
			return;
		}
		auto& myNode = GetNodeInfo();

		auto& nodeRegistry = tlsRegistryManager.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
		auto& nodeList = *nodeRegistry[myNode.node_type()].mutable_node_list();
		for (auto it = nodeList.begin(); it != nodeList.end(); ++it) {
			if (IsMyNode(*it)) {
				return ;
			}
		}

		gNode->GetEtcdManager().RequestEtcdLease();
		}
	);
}


