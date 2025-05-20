#include "node.h"
#include <grpcpp/create_channel.h>
#include "all_config.h"
#include "config_loader/config.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/message_differencer.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "log/constants/log_constants.h"
#include "log/system/console_log_system.h"
#include "logic/constants/node.pb.h"
#include "logic/event/server_event.pb.h"
#include "muduo/base/TimeZone.h"
#include "network/rpc_session.h"
#include "service_info/service_info.h"
#include "thread_local/storage_common_logic.h"
#include "time/system/time_system.h"
#include "util/network_utils.h"
#include "node/comp/node_comp.h"
#include "node/system/node_system.h"
#include "network/process_info.h"
#include "etcd_helper.h"
#include "grpc_client_system.h"
#include "proto/logic/event/node_event.pb.h"
#include "service_info/centre_service_service_info.h"
#include "service_info/game_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "pbc/common_error_tip.pb.h"
#include "util/random.h"
#include <regex>

Node::Node(muduo::net::EventLoop* eventLoop, const std::string& logFilePath)
    : loop_(eventLoop),
      muduoLog(logFilePath, kMaxLogFileRollSize, 1) {
    // Initialize node's event loop and logging system
    LOG_INFO << "Node created with event loop and log file path: " << logFilePath;
}

Node::~Node() {
    LOG_INFO << "Node is being destroyed.";
    ShutdownNode();  // Clean up node resources upon destruction
}

NodeInfo& Node::GetNodeInfo() const {
    return tls.globalRegistry.get_or_emplace<NodeInfo>(GlobalEntity());
}

std::string Node::GetServiceName(uint32_t nodeType) const {
    return eNodeType_Name(nodeType) + ".rpc";
}

void Node::Initialize() {
    LOG_TRACE << "Initializing node...";
    RegisterEventHandlers();
    LoadConfigurationFiles();        // Load configuration files
    SetUpRpcServer();                // Set up the RPC server
    SetUpLoggingSystem();            // Set up logging system
    ConfigureEnvironment();          // Set environment settings
    LoadConfigurationData();        // Load the configuration data
    InitializeGrpcClients();        // Initialize gRPC clients
    InitializeGrpcMessageQueues();  // Initialize gRPC queues for async processing
    SetUpEventHandlers();           // Set up event handlers
    AcquireNodeLease();
    LOG_TRACE << "Node initialization complete.";
}

// Sets up the RPC server for the node
void Node::SetUpRpcServer() {
    auto& nodeInfo = GetNodeInfo();
    nodeInfo.mutable_endpoint()->set_ip(localip());
    nodeInfo.mutable_endpoint()->set_port(get_available_port(GetNodeType() * 10000));
    nodeInfo.set_scene_node_type(tlsCommonLogic.GetGameConfig().scene_node_type());
    nodeInfo.set_node_type(GetNodeType());
    nodeInfo.set_protocol_type(PROTOCOL_TCP);
    nodeInfo.set_launch_time(TimeUtil::NowSecondsUTC());
    nodeInfo.set_zone_id(tlsCommonLogic.GetGameConfig().zone_id());

    InetAddress serviceAddr(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port());
    rpcServer = std::make_unique<RpcServerPtr::element_type>(loop_, serviceAddr);
    rpcServer->start();
    LOG_INFO << "Setting up RPC server with local IP: " << localip() << " and port: " << GetPort();
}

// Starts the RPC server and begins service node watching
void Node::StartRpcServer() {
    FetchServiceNodes();
    StartWatchingServiceNodes();
    tls.dispatcher.trigger<OnServerStart>();  // Trigger server start event
    LOG_TRACE << "RPC server started at " << GetNodeInfo().DebugString();
}

// Gracefully shuts down the node and releases resources
void Node::ShutdownNode() {
    LOG_TRACE << "Shutting down node...";
    StopWatchingServiceNodes();  // Stop watching all service nodes
    tls.Clear();                  // Clear thread-local storage
    muduoLog.stop();              // Stop logging system
    ReleaseNodeId();              // Release the node ID

    // Cancel all timers
    renewNodeLeaseTimer.Cancel();
    etcdQueueTimer.Cancel();
    LOG_TRACE << "Timers canceled and resources released.";

    tls.Clear();
    // Close all gRPC connections
}

// Sets up the logging system with appropriate configurations
void Node::SetUpLoggingSystem() {
    muduo::Logger::setLogLevel(static_cast<muduo::Logger::LogLevel>(tlsCommonLogic.GetBaseDeployConfig().log_level()));
    muduo::Logger::setOutput(AsyncOutput);
    muduoLog.start();  // Start logging
}

void Node::RegisterEventHandlers() {
    tls.dispatcher.sink<OnConnected2TcpServerEvent>().connect<&Node::OnConnectedToServer>(*this);
    tls.dispatcher.sink<OnTcpClientConnectedEvent>().connect<&Node::OnClientConnected>(*this);
}

// Loads configuration files for the node
void Node::LoadConfigurationFiles() {
    readBaseDeployConfig("etc/base_deploy_config.yaml", tlsCommonLogic.GetBaseDeployConfig());
    readGameConfig("etc/game_config.yaml", tlsCommonLogic.GetGameConfig());
}

// Loads and processes all configuration data
void Node::LoadConfigurationData() {
    LoadAllConfig();
    LoadAllConfigAsyncWhenServerLaunch();
    OnConfigLoadSuccessful();
}

// Configures environment variables, such as time zone and node info
void Node::ConfigureEnvironment() {
    const muduo::TimeZone tz("zoneinfo/Asia/Hong_Kong");
    muduo::Logger::setTimeZone(tz);  // Set timezone to Hong Kong

    tls.globalNodeRegistry.emplace<ServiceNodeList>(GetGlobalGrpcNodeEntity());
}

// Initializes gRPC clients and stubs for service communication
void Node::InitializeGrpcClients() {
    GrpcClientSystem::InitEtcdStubs(tlsCommonLogic.GetBaseDeployConfig().etcd_hosts());
}

// Initializes the gRPC queues for async message handling
void Node::InitializeGrpcMessageQueues() {
    InitetcdserverpbKVCompletedQueue(tls.globalNodeRegistry, GetGlobalGrpcNodeEntity());
    InitetcdserverpbWatchCompletedQueue(tls.globalNodeRegistry, GetGlobalGrpcNodeEntity());
    InitetcdserverpbLeaseCompletedQueue(tls.globalNodeRegistry, GetGlobalGrpcNodeEntity());

    // Periodically handle etcd server responses
    etcdQueueTimer.RunEvery(0.001, []() {
		HandleetcdserverpbKVCompletedQueueMessage(tls.globalNodeRegistry);
		HandleetcdserverpbWatchCompletedQueueMessage(tls.globalNodeRegistry);
		HandleetcdserverpbLeaseCompletedQueueMessage(tls.globalNodeRegistry);
    });
}

std::string Node::MakeServiceNodeEtcdKey(const NodeInfo& nodeInfo) {
    // 构造 etcd 中的键名，结构如下：
    // {serviceName}/zone/{zone_id}/node_type/{node_type}/node_id/{node_id}
    return GetServiceName(nodeInfo.node_type()) +
           "/zone/" + std::to_string(nodeInfo.zone_id()) +
           "/node_type/" + std::to_string(nodeInfo.node_type()) +
           "/node_id/" + std::to_string(nodeInfo.node_id());
}

// Registers the current node with the service registry
void Node::RegisterSelfInService() {
    EtcdHelper::PutServiceNodeInfo(GetNodeInfo(), MakeServiceNodeEtcdKey(GetNodeInfo()));
}

// Fetches all service nodes from etcd
void Node::FetchServiceNodes() {
    for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
        EtcdHelper::RangeQuery(prefix);  // Query and fetch service node info from etcd
    }
}

// Starts watching for new service nodes in etcd
void Node::StartWatchingServiceNodes() {
    for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
        EtcdHelper::StartWatchingPrefix(prefix);  // Start watching the service prefix
    }
}

// Stops watching for service nodes in etcd
void Node::StopWatchingServiceNodes() {
    EtcdHelper::StopAllWatching();  // Stop watching all service nodes
}

void Node::ConnectToNode(const NodeInfo& nodeInfo) {
    switch (nodeInfo.protocol_type()) {
    case PROTOCOL_GRPC:
        ConnectToGrpcNode(nodeInfo);
        break;
    case PROTOCOL_TCP:
        ConnectToTcpNode(nodeInfo);
        break;
    case PROTOCOL_HTTP:
        ConnectToHttpNode(nodeInfo);
        break;
    default:
        LOG_ERROR << "Unsupported protocol type: " << nodeInfo.protocol_type()
                  << " for node: " << nodeInfo.DebugString();
        break;
    }
}

void Node::ConnectToGrpcNode(const NodeInfo& nodeInfo) {
    auto& serviceNodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
    auto& registry = NodeSystem::GetRegistryForNodeType(nodeInfo.node_type());

    const entt::entity id{ nodeInfo.node_id() };
    auto nodeId = registry.create(id);
    if (nodeId != id) {
        LOG_ERROR << "Login node not found for entity: " << entt::to_integral(nodeId);
        return;
    }

    registry.emplace<std::shared_ptr<grpc::Channel>>(nodeId,
        grpc::CreateChannel(::FormatIpAndPort(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port()),
                            grpc::InsecureChannelCredentials()));

    ProcessGrpcNode(nodeInfo);
}

void Node::ConnectToTcpNode(const NodeInfo& nodeInfo) {
    auto& registry = NodeSystem::GetRegistryForNodeType(nodeInfo.node_type());

    entt::entity id{ nodeInfo.node_id() };

    // Check if the node already exists  
    if (registry.valid(id)) {
        if (auto* existingNodeInfo = registry.try_get<NodeInfo>(id);
            existingNodeInfo && existingNodeInfo->node_id() == nodeInfo.node_id()) {
            LOG_TRACE << "Node with ID " << nodeInfo.node_id() << " already exists and matches. Skipping creation.";
            return;  // If node exists and matches, skip
        }
        else {
            LOG_TRACE << "Node with ID " << nodeInfo.node_id() << " exists but does not match the provided NodeInfo!";
            return;  // Serious issue, node exists but does not match
        }
    }

    // If node does not exist, create new node  
    const auto nodeId = registry.create(id);
    if (nodeId != id) {
        LOG_ERROR << "Failed to create node entity: " << entt::to_integral(nodeId);
        return;
    }

    InetAddress endpoint(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port());
    auto& node = registry.emplace<RpcClient>(nodeId, loop_, endpoint);
    registry.emplace<NodeInfo>(id, nodeInfo);

    // Register service and connect  
    node.registerService(GetNodeReplyService());
    node.connect();

    // Check if it is the central node of the current zone  
    if (nodeInfo.node_type() == CentreNodeService &&
        nodeInfo.zone_id() == tlsCommonLogic.GetGameConfig().zone_id()) {
        zoneCentreNode = &node;
    }
}

void Node::ConnectToHttpNode(const NodeInfo& nodeInfo) {
    // HTTP connection logic here if needed
}

void Node::ReleaseNodeId() {
    EtcdHelper::RevokeLeaseAndCleanup(static_cast<int64_t>(GetNodeInfo().lease_id()));
}

void Node::SetUpEventHandlers() {
    InitMessageInfo();
    InitializeGrpcResponseHandlers();

	void InitRepliedHandler();
	InitRepliedHandler();
}

void Node::AsyncOutput(const char* msg, int len) {
    logger().append(msg, len);
#ifdef WIN32
    LogToConsole(msg, len);  // Output to console on Windows
#endif
}

void Node::InitGrpcClients() {
    GrpcClientSystem::InitEtcdStubs(tlsCommonLogic.GetBaseDeployConfig().etcd_hosts());
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


void Node::AddServiceNode(const std::string& nodeJson, uint32_t nodeType) {
	LOG_INFO << "Adding service node of type " << nodeType << " with JSON: " << nodeJson;

	if (!eNodeType_IsValid(static_cast<int32_t>(nodeType))) {
		LOG_ERROR << "Invalid node type: " << nodeType;
		return;
	}

	NodeInfo newNodeInfo;

	auto parseResult = google::protobuf::util::JsonStringToMessage(nodeJson, &newNodeInfo);
	if (!parseResult.ok()) {
		LOG_ERROR << "Failed to parse JSON for nodeType: " << nodeType
				  << ", JSON: " << nodeJson
				  << ", Error: " << parseResult.message().data();
		return;
	}

	auto& nodeRegistry = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& nodeList = *nodeRegistry[nodeType].mutable_node_list();

	for (const auto& existingNode : nodeList) {
		if (existingNode.lease_id() == newNodeInfo.lease_id()) {
			LOG_INFO << "Node already exists with IP: " << existingNode.endpoint().ip()
					 << ", Port: " << existingNode.endpoint().port();
			return;
		}
	}

	*nodeList.Add() = newNodeInfo;

	LOG_INFO << "Node added. Type: " << nodeType << ", Info: " << newNodeInfo.DebugString();

	if (!targetNodeTypeWhitelist.contains(nodeType)) {
		return;
	}

	auto isNodeAlreadyRegistered = [&]() -> bool {
		entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);
		for (const auto& [entity, client, nodeInfo] : registry.view<RpcClient, NodeInfo>().each()) {
			if (nodeInfo.endpoint().ip() == newNodeInfo.endpoint().ip() &&
				nodeInfo.endpoint().port() == newNodeInfo.endpoint().port()) {
				LOG_INFO << "Node already registered with IP: " << nodeInfo.endpoint().ip()
						 << ", Port: " << nodeInfo.endpoint().port();
				return true;
				}
		}
		return false;
	};

	if (isNodeAlreadyRegistered()) {
		return;
	}

	ConnectToNode(newNodeInfo);
	LOG_INFO << "Connected to node: " << newNodeInfo.DebugString();
}


inline bool IsTcpNodeInfoType(int nodeType) {
	static const std::unordered_set<int> validTypes = {
	CentreNodeService,
	SceneNodeService,
	GateNodeService
	};
	return validTypes.contains(nodeType);
}

void Node::HandleServiceNodeStart(const std::string& key, const std::string& value) {
	LOG_DEBUG << "Handling service node start. Key: " << key << ", Value: " << value;

	if (const auto nodeType = NodeSystem::GetServiceTypeFromPrefix(key); eNodeType_IsValid(nodeType)) {
		AddServiceNode(value, nodeType);
	} else {
		LOG_ERROR << "Unknown service type for key: " << key;
	}
}

void Node::HandleServiceNodeStop(const std::string& key, const std::string& value) {
	LOG_DEBUG << "Handling service node stop. Key: " << key << ", Value: " << value;

	const auto nodeType = NodeSystem::GetServiceTypeFromPrefix(key);

	if (!eNodeType_IsValid(nodeType)) {
		LOG_TRACE << "Unknown service type for key: " << key;
		return;
	}

	std::regex pattern(R"(.*?/node_type/(\d+)/node_id/(\d+))");
	std::smatch match;

	if (!std::regex_match(key, match, pattern)) {
		return;
	}

	uint32_t nodeId = std::stoul(match[2]);

	if (nodeId > std::numeric_limits<uint32_t>::max()) {
		LOG_ERROR << "Value exceeds uint32_t limit.";
		return;
	}

	auto& nodeRegistry = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& nodeList = *nodeRegistry[nodeType].mutable_node_list();

	for (auto it = nodeList.begin(); it != nodeList.end(); ) {
		if (it->node_type() == nodeType && it->node_id() == nodeId) {
			LOG_INFO << "Removing old node with lease_id: " << it->lease_id();
			it = nodeList.erase(it);
			break;
		} else {
			++it;
		}
	}

	ProcessNodeStop(nodeType, nodeId);

	entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);
	Destroy(registry, entt::entity{nodeId});

	LOG_INFO << "Service node stopped. ID: " << nodeId;
}

void Node::InitializeGrpcResponseHandlers() {
	// gRPC KV Range Response Handler
	AsyncetcdserverpbKVRangeHandler = [this](const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>& call) {
		if (call->status.ok()) {
			for (const auto& kv : call->reply.kvs()) {
				HandleServiceNodeStart(kv.key(), kv.value());
			}
		} else {
			LOG_ERROR << "RPC failed: " << call->status.error_message();
		}
	};

	// gRPC Put Response Handler
	AsyncetcdserverpbKVPutHandler = [this](const std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall>& call) {
		LOG_DEBUG << "Put response: " << call->reply.DebugString();
		StartWatchingServiceNodes();
	};

	// gRPC Delete Response Handler
	AsyncetcdserverpbKVDeleteRangeHandler = [](const std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall>& call) {
		// Reserved for future use
	};

	// gRPC Transaction Handler
	AsyncetcdserverpbKVTxnHandler = [this](const std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall>& call) {
		if (call->status.ok()) {
			LOG_DEBUG << "Transaction response: " << call->reply.DebugString();
			call->reply.succeeded() ? StartRpcServer() : AcquireNode();
		} else {
			LOG_ERROR << "RPC failed: " << call->status.error_message();
		}
	};

	// gRPC Watch Handler
	AsyncetcdserverpbWatchWatchHandler = [this](const ::etcdserverpb::WatchResponse& response) {
		if (response.created()) {
			LOG_TRACE << "Watch created.";
			return;
		}

		if (response.canceled()) {
			LOG_INFO << "Watch canceled. Reason: " << response.cancel_reason();
			if (response.compact_revision() > 0) {
				LOG_ERROR << "Revision " << response.compact_revision() << " compacted.";
			}
			return;
		}

		for (const auto& event : response.events()) {
			if (event.type() == mvccpb::Event_EventType::Event_EventType_PUT) {
				LOG_INFO << "Key put: " << event.kv().key();
				HandleServiceNodeStart(event.kv().key(), event.kv().value());
			} else if (event.type() == mvccpb::Event_EventType::Event_EventType_DELETE) {
				HandleServiceNodeStop(event.kv().key(), event.kv().value());
				LOG_INFO << "Key deleted: " << event.kv().key();
			}
		}
	};

	// gRPC Lease Grant Handler
	AsyncetcdserverpbLeaseLeaseGrantHandler = [this](const std::unique_ptr<AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall>& call) {
		if (call->status.ok()) {
			GetNodeInfo().set_lease_id(call->reply.id());
			KeepNodeAlive();
			AcquireNode();
			LOG_INFO << "Lease granted: " << call->reply.DebugString();
		} else {
			LOG_ERROR << "RPC failed: " << call->status.error_message();
		}
	};
}

void Node::OnConnectedToServer(const OnConnected2TcpServerEvent& event) {
	auto& connection = event.conn_;
	if (!connection->connected()) {
		LOG_INFO << "Client disconnected: " << connection->peerAddress().toIpPort();
		return;
	}

	LOG_INFO << "Connected to server: " << connection->peerAddress().toIpPort();
	RegisterNodeSessions(connection);
}

// Handle connections for different node types

// 全局静态变量，存储 NodeType 到 MessageId 的映射
// 全局静态数组，存储 NodeType 到 MessageId 的映射
static uint32_t kNodeTypeToMessageId[eNodeType_MAX] = {
	0,
	0,
	CentreServiceRegisterNodeSessionMessageId,  // 对应 kCentreNode
	GameServiceRegisterNodeSessionMessageId,    // 对应 kSceneNode
	GateServiceRegisterNodeSessionMessageId     // 对应 kGateNode
};

void Node::RegisterNodeSessions(const muduo::net::TcpConnectionPtr& connection) {
	AttemptNodeRegistration(CentreNodeService, connection);
	AttemptNodeRegistration(SceneNodeService, connection);
	AttemptNodeRegistration(GateNodeService, connection);
}

void Node::AttemptNodeRegistration(uint32_t nodeType, const muduo::net::TcpConnectionPtr& connection) const {
	entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);

	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClient, NodeInfo>().each()) {
		if (!IsSameAddress(client.peer_addr(), connection->peerAddress())) {
			continue;
		}

		LOG_INFO << "Matching peer address found in " << tls.GetRegistryName(registry)
				 << ": " << connection->peerAddress().toIpPort();

		registry.emplace<TimerTaskComp>(entity).RunAfter(0.5, [connection, this, nodeType, &client]() {
			RegisterNodeSessionRequest request;
			request.mutable_self_node()->CopyFrom(GetNodeInfo());
			request.mutable_endpoint()->set_ip(connection->localAddress().toIp());
			request.mutable_endpoint()->set_port(connection->localAddress().port());

			client.CallRemoteMethod(kNodeTypeToMessageId[nodeType], request);
		});

		return;
	}
}

void Node::OnClientConnected(const OnTcpClientConnectedEvent& event) {
	auto& connection = event.conn_;
	if (!connection->connected()) {
		for (const auto& [entity, session] : tls.sessionRegistry.view<RpcSession>().each()) {
			auto& existingConn = session.connection;
			if (!IsSameAddress(connection->peerAddress(), existingConn->peerAddress())) {
				LOG_TRACE << "Endpoint mismatch: expected IP = " << connection->peerAddress().toIp()
					<< ", port = " << connection->peerAddress().port()
					<< "; actual IP = " << existingConn->peerAddress().toIp()
					<< ", port = " << existingConn->peerAddress().port();
				continue;
			}
			tls.sessionRegistry.destroy(entity);
			return;
		}
		return;
	}

	auto entity = tls.sessionRegistry.create();
	tls.sessionRegistry.emplace<RpcSession>(entity, RpcSession{ connection });

	LOG_INFO << "Client connected: " << connection->peerAddress().toIpPort();
}

void Node::HandleNodeRegistration(
	const RegisterNodeSessionRequest& request,
	RegisterNodeSessionResponse& response
) const {
	auto& peerNodeInfo = request.self_node();
	response.mutable_peer_node()->CopyFrom(GetNodeInfo());

	LOG_TRACE << "Received node registration request: " << request.DebugString();

	auto tryRegisterToRegistry = [&](const TcpConnectionPtr& connection, uint32_t nodeType) -> bool {
		const auto& serviceNodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());

		for (auto& serverNodeInfo : serviceNodeList[nodeType].node_list()) {
			if (serverNodeInfo.lease_id() != peerNodeInfo.lease_id()) {
				continue;
			}

			entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);
			entt::entity entity = registry.create(entt::entity{ peerNodeInfo.node_id() });

			if (entity != entt::entity{ peerNodeInfo.node_id() }) {
				LOG_ERROR << "Failed to create node entity in " << tls.GetRegistryName(registry);
				return false;
			}

			registry.emplace<RpcSession>(entity, RpcSession{ connection });
			LOG_INFO << "Node with ID " << peerNodeInfo.node_id() << " registered in " << tls.GetRegistryName(registry);
			return true;
		}

		return false;
	};

	for (const auto& [entity, session] : tls.sessionRegistry.view<RpcSession>().each()) {
		auto& connection = session.connection;
		if (!IsSameAddress(connection->peerAddress(), muduo::net::InetAddress(request.endpoint().ip(), request.endpoint().port()))) {
			continue;
		}

		for (uint32_t nodeType = eNodeType_MIN; nodeType < eNodeType_MAX; ++nodeType) {
			if (GetNodeType() == nodeType || !IsTcpNodeInfoType(nodeType)) {
				continue;
			}

			if (tryRegisterToRegistry(connection, nodeType)) {
				tls.sessionRegistry.destroy(entity);
				response.mutable_error_message()->set_id(kCommon_errorOK);
				return;
			}
		}
	}

	response.mutable_error_message()->set_id(kFailedToRegisterTheNode);
}

void TriggerNodeConnectionEvent(entt::registry& registry, const RegisterNodeSessionResponse& response) {
	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClient, NodeInfo>().each()) {
		if (client.peer_addr().toIp() != response.peer_node().endpoint().ip() ||
			client.peer_addr().port() != response.peer_node().endpoint().port()) {
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
			LOG_INFO << "CentreNode connected. Entity: " << entt::to_integral(entity);
		}

		registry.remove<TimerTaskComp>(entity);
		break;
	}
}


void Node::HandleNodeRegistrationResponse(const RegisterNodeSessionResponse& response) const {
	LOG_INFO << "Received node registration response: " << response.DebugString();

	uint32_t nodeType = response.peer_node().node_type();
	entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);

	if (response.error_message().id() != kCommon_errorOK) {
		LOG_TRACE << "Failed registration: " << response.DebugString();

		for (const auto& [entity, client, nodeInfo] : registry.view<RpcClient, NodeInfo>().each()) {
			if (!IsSameAddress(client.peer_addr(), muduo::net::InetAddress(
				response.peer_node().endpoint().ip(),
				response.peer_node().endpoint().port()))) {
				continue;
				}

			registry.get<TimerTaskComp>(entity).RunAfter(0.5, [this, &client, nodeType]() {
				RegisterNodeSessionRequest request;
				*request.mutable_self_node() = GetNodeInfo();
				request.mutable_endpoint()->set_ip(client.local_addr().toIp());
				request.mutable_endpoint()->set_port(client.local_addr().port());
				client.CallRemoteMethod(kNodeTypeToMessageId[nodeType], request);
			});
			return;
		}
		return;
	}

	entt::entity peerEntity{ response.peer_node().node_id() };
	registry.remove<TimerTaskComp>(peerEntity);

	TriggerNodeConnectionEvent(registry, response);
	LOG_INFO << "Node registration successful.";
}

void Node::AcquireNode() {
	auto& nodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity())[GetNodeType()];
	auto& protoList = *nodeList.mutable_node_list();

	uint32_t maxNodeId = 0;
	UInt32Set occupiedNodeIds;

	for (const auto& node : protoList) {
		maxNodeId = std::max(maxNodeId, node.node_id());
		occupiedNodeIds.insert(node.node_id());
	}

	constexpr uint32_t randomOffset = 5;
	uint32_t searchLimit = maxNodeId + tlsCommonLogic.GetRng().Rand<uint32_t>(0, randomOffset);
	uint32_t assignedId = 0;

	for (uint32_t id = 0; id < searchLimit; ++id) {
		if (!occupiedNodeIds.contains(id)) {
			assignedId = id;
			break;
		}
	}

	GetNodeInfo().set_node_id(assignedId);
	const auto nodeKey = MakeServiceNodeEtcdKey(GetNodeInfo());

	EtcdHelper::PutIfAbsent(nodeKey, GetNodeInfo());
}

void Node::KeepNodeAlive() {
	renewNodeLeaseTimer.RunEvery(tlsCommonLogic.GetBaseDeployConfig().lease_renew_interval(), [this]() {
		etcdserverpb::LeaseKeepAliveRequest request;
		request.set_id(static_cast<int64_t>(GetNodeInfo().lease_id()));
		SendetcdserverpbLeaseLeaseKeepAlive(tls.globalNodeRegistry, GetGlobalGrpcNodeEntity(), request);
	});
}

void Node::AcquireNodeLease() {
	uint64_t ttl = tlsCommonLogic.GetBaseDeployConfig().node_ttl_seconds();
	EtcdHelper::GrantLease(ttl);
}


