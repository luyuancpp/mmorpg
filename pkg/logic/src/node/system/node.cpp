#include "node.h"
#include <grpcpp/create_channel.h>
#include "all_config.h"
#include "config_loader/config.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/message_differencer.h"
#include "grpc/generator/proto/common/deploy_service_grpc.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "log/constants/log_constants.h"
#include "log/system/console_log_system.h"
#include "logic/constants/node.pb.h"
#include "logic/event/server_event.pb.h"
#include "muduo/base/TimeZone.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "proto/common/deploy_service.grpc.pb.h"
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

Node::Node(muduo::net::EventLoop* loop, const std::string& logFilePath)
	: loop_(loop),
	muduoLog(logFilePath, kMaxLogFileRollSize, 1) {
	// Initialize node's event loop and logging system
	LOG_INFO << "Node created with event loop and log file path: " << logFilePath;
}

Node::~Node() {
	LOG_INFO << "Node is being destroyed.";
	ShutdownNode();  // Clean up node resources upon destruction
}

void Node::InitializeDeploymentService(const std::string& service_address)
{
	LOG_INFO << "Initializing deployment service with address: " << service_address;
	tls.globalNodeRegistry.emplace<GrpcDeployServiceStubPtr>(GlobalGrpcNodeEntity())
		= DeployService::NewStub(grpc::CreateChannel(service_address, grpc::InsecureChannelCredentials()));

	// Request to fetch node information
	NodeInfoRequest request;
	request.set_node_type(GetNodeType());  // Use subclass to specify node type
	LOG_INFO << "Sending deployment service request for node type: " << GetNodeType();
	SendDeployServiceGetNodeInfo(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);

	// Periodically renew the node lease
	renewNodeLeaseTimer.RunEvery(tlsCommonLogic.GetBaseDeployConfig().lease_renew_interval(), [this]() {
		RenewLeaseIDRequest request;
		request.set_lease_id(GetNodeInfo().lease_id());
		LOG_TRACE << "Renewing lease with ID: " << GetNodeInfo().lease_id();
		SendDeployServiceRenewLease(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);
		});
}

void Node::Initialize() {
	LOG_INFO << "Initializing node...";
	RegisterEventHandlers();
	LoadConfigurationFiles();        // Load configuration files
	SetupRpcServer();                // Set up the RPC server
	SetupLoggingSystem();            // Set up logging system
	ConfigureEnvironment();          // Set environment settings
	LoadConfigurationData();        // Load the configuration data
	InitializeGrpcClients();        // Initialize gRPC clients
	InitializeGrpcMessageQueues();  // Initialize gRPC queues for async processing
	RegisterServiceNodes();         // Fetch and register service nodes
	SetUpEventHandlers();           // Set up event handlers
	LOG_INFO << "Node initialization complete.";
}

// Sets up the RPC server for the node
void Node::SetupRpcServer() {
	LOG_INFO << "Setting up RPC server with local IP: " << localip() << " and port: " << GetPort();
	GetNodeInfo().mutable_endpoint()->set_ip(localip());
	GetNodeInfo().mutable_endpoint()->set_port(get_available_port(GetNodeType() * 10000));

	InetAddress service_addr(GetNodeInfo().endpoint().ip(), GetNodeInfo().endpoint().port());
	rpcServer = std::make_unique<RpcServerPtr::element_type>(loop_, service_addr);
	rpcServer->start();
}

// Starts the RPC server and begins service node watching
void Node::StartRpcServer() {
	LOG_INFO << "Starting RPC server...";
	tls.dispatcher.trigger<OnServerStart>();  // Trigger server start event

	deployQueueTimer.Cancel();  // Stop deploy queue timer
	LOG_INFO << "Deploy queue timer canceled.";

	RegisterSelfInService();     // Register this node in service registry
	StartWatchingServiceNodes();  // Start watching for new service nodes

	LOG_INFO << "RPC server started at " << GetNodeInfo().DebugString();

}

// Gracefully shuts down the node and releases resources
void Node::ShutdownNode() {
	LOG_INFO << "Shutting down node...";
	StopWatchingServiceNodes();  // Stop watching all service nodes
	tls.Clear();                  // Clear thread-local storage
	muduoLog.stop();              // Stop logging system
	ReleaseNodeId();              // Release the node ID

	// Cancel all timers
	deployQueueTimer.Cancel();
	renewNodeLeaseTimer.Cancel();
	etcdQueueTimer.Cancel();
	LOG_INFO << "Timers canceled and resources released.";

	// Close all gRPC connections
}

// Sets up the logging system with appropriate configurations
void Node::SetupLoggingSystem() {
	muduo::Logger::setLogLevel(static_cast<muduo::Logger::LogLevel>(tlsCommonLogic.GetBaseDeployConfig().log_level()));
	muduo::Logger::setOutput(AsyncOutput);
	muduoLog.start();  // Start logging
}

void Node::RegisterEventHandlers()
{
	tls.dispatcher.sink<OnConnected2TcpServerEvent>().connect<&Node::OnConnectedToServer>(*this);
	tls.dispatcher.sink<OnBeConnectedEvent>().connect<&Node::OnClientConnected>(*this);
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

	GetNodeInfo().set_launch_time(TimeUtil::NowSecondsUTC());
	GetNodeInfo().set_scene_node_type(tlsCommonLogic.GetGameConfig().scene_node_type());
	GetNodeInfo().set_node_type(GetNodeType());
	GetNodeInfo().set_zone_id(tlsCommonLogic.GetGameConfig().zone_id());

	tls.globalNodeRegistry.emplace<ServiceNodeList>(GlobalGrpcNodeEntity());
}

// Initializes gRPC clients and stubs for service communication
void Node::InitializeGrpcClients() {
	GrpcClientSystem::InitEtcdStubs(tlsCommonLogic.GetBaseDeployConfig().etcd_hosts());
}

// Initializes the gRPC queues for async message handling
void Node::InitializeGrpcMessageQueues() {
	InitDeployServiceCompletedQueue(tls.globalNodeRegistry, GlobalGrpcNodeEntity());
	InitetcdserverpbKVCompletedQueue(tls.globalNodeRegistry, GlobalGrpcNodeEntity());
	InitetcdserverpbWatchCompletedQueue(tls.globalNodeRegistry, GlobalGrpcNodeEntity());
	InitetcdserverpbLeaseCompletedQueue(tls.globalNodeRegistry, GlobalGrpcNodeEntity());

	// Periodically handle deploy service completed messages
	deployQueueTimer.RunEvery(0.001, []() {
		HandleDeployServiceCompletedQueueMessage(tls.globalNodeRegistry);
		});

	// Periodically handle etcd server responses
	etcdQueueTimer.RunEvery(0.001, []() {
		HandleetcdserverpbKVCompletedQueueMessage(tls.globalNodeRegistry);
		HandleetcdserverpbWatchCompletedQueueMessage(tls.globalNodeRegistry);
		HandleetcdserverpbLeaseCompletedQueueMessage(tls.globalNodeRegistry);
		});
}

// Registers the current node with the service registry
void Node::RegisterSelfInService() {
	EtcdHelper::PutServiceNodeInfo(GetNodeInfo(), GetServiceName());
}

// Fetches and registers all service nodes from etcd
void Node::RegisterServiceNodes() {
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

void Node::ConnectToNode(entt::registry& registry, const NodeInfo& nodeInfo)
{
	entt::entity id{ nodeInfo.node_id() };

	// 检查节点是否已经存在  
	if (registry.valid(id)) {
		auto* existingNodeInfo = registry.try_get<NodeInfo>(id);
		if (existingNodeInfo && existingNodeInfo->node_id() == nodeInfo.node_id()) {
			LOG_INFO << "Node with ID " << nodeInfo.node_id() << " already exists and matches. Skipping creation.";
			return;  // 如果节点已经存在且匹配，则直接返回  
		}
		else {
			LOG_ERROR << "Node with ID " << nodeInfo.node_id() << " exists but does not match the provided NodeInfo!";
			return;  // 严重问题，节点存在但信息不匹配  
		}
	}

	// 如果节点不存在，则创建新节点  
	const auto nodeId = registry.create(id);
	if (nodeId != id) {
		LOG_ERROR << "Failed to create node entity: " << entt::to_integral(nodeId);
		return;  // 如果创建失败，则跳过该中心节点  
	}

	InetAddress endpoint(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port());
	auto& node = registry.emplace<RpcClient>(nodeId, loop_, endpoint);
	registry.emplace<NodeInfo>(id, nodeInfo);

	// 注册服务并连接  
	node.registerService(GetNodeRepleyService());
	node.connect();

	// 判断是否为当前区域的中心节点  
	if (nodeInfo.node_type() == kCentreNode &&
		nodeInfo.zone_id() == tlsCommonLogic.GetGameConfig().zone_id()) {
		zoneCentreNode = &node;
	}
}

void Node::ReleaseNodeId() {
	ReleaseIDRequest request;
	request.set_id(GetNodeId());  // 获取节点 ID
	request.set_node_type(GetNodeType());  // 获取节点类型
	SendDeployServiceReleaseID(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);  // 释放节点 ID
}

void Node::SetUpEventHandlers()
{
	InitMessageInfo();

	void InitGrpcDeployServiceResponseHandler();
	InitGrpcDeployServiceResponseHandler();

	InitializeGrpcResponseHandlers();

	void InitRepliedHandler();
	InitRepliedHandler();
}

void Node::AsyncOutput(const char* msg, int len) {
	logger().append(msg, len);
#ifdef WIN32
	Log2Console(msg, len);  // 在 Windows 系统上输出到控制台
#endif
}

void Node::InitGrpcClients() {
	GrpcClientSystem::InitEtcdStubs(tlsCommonLogic.GetBaseDeployConfig().etcd_hosts());
}

std::string Node::FormatIpAndPort()
{
	return ::FormatIpAndPort(GetNodeInfo().endpoint().ip(), GetNodeInfo().endpoint().port());
}

std::string Node::GetIp()
{
	return GetNodeInfo().endpoint().ip();
}

uint32_t Node::GetPort()
{
	return GetNodeInfo().endpoint().port();
}

void Node::AddServiceNode(const std::string& nodeJson, uint32_t nodeType) {
    LOG_INFO << "Adding service node of type " << nodeType << " with JSON: " << nodeJson;

    // Validate the node type  
    if (!eNodeType_IsValid(nodeType)) {
        LOG_ERROR << "Invalid node type: " << nodeType;
        return;
    }

    NodeInfo newNodeInfo;

    // Parse the JSON string into NodeInfo protobuf message  
    auto result = google::protobuf::util::JsonStringToMessage(nodeJson, &newNodeInfo);
    if (!result.ok()) {
        LOG_ERROR << "Failed to parse JSON for nodeType: " << nodeType
                  << ", JSON: " << nodeJson
                  << ", Error: " << result.message().data();
        return;
    }

    // Check if the node already exists in the corresponding node registry's RpcClient components
    auto checkRegistryForDuplicate = [&](auto& registry) {
        for (const auto& [e, client, nodeInfo] : registry.view<RpcClient, NodeInfo>().each()) {
            if (nodeInfo.endpoint().ip() == newNodeInfo.endpoint().ip() &&
                nodeInfo.endpoint().port() == newNodeInfo.endpoint().port()) {
                LOG_INFO << "Node with endpoint IP: " << nodeInfo.endpoint().ip()
                         << " and Port: " << nodeInfo.endpoint().port()
                         << " already exists in the registry. Skipping addition.";
                return true;
            }
        }
        return false;
    };

    if ((nodeType == kCentreNode && checkRegistryForDuplicate(tls.centreNodeRegistry)) ||
        (nodeType == kSceneNode && checkRegistryForDuplicate(tls.sceneNodeRegistry)) ||
        (nodeType == kGateNode && checkRegistryForDuplicate(tls.gateNodeRegistry))) {
        return;
    }

    if (!GetAllowedTargetNodeTypes().contains(nodeType)) {
        return;
    }

    // Connect to the node based on its type  
    if (nodeType == kCentreNode) {
        ConnectToNode(tls.centreNodeRegistry, newNodeInfo);
        LOG_INFO << "Connected to center node: " << newNodeInfo.DebugString();
    } else if (nodeType == kSceneNode) {
        ConnectToNode(tls.sceneNodeRegistry, newNodeInfo);
        LOG_INFO << "Connected to scene node: " << newNodeInfo.DebugString();
    } else if (nodeType == kGateNode) {
        ConnectToNode(tls.gateNodeRegistry, newNodeInfo);
        LOG_INFO << "Connected to gate node: " << newNodeInfo.DebugString();
    }

	auto& serviceNodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GlobalGrpcNodeEntity());
	auto& nodeList = *serviceNodeList[nodeType].mutable_node_list();
	*nodeList.Add() = newNodeInfo;
}

// Handles the start of a service node
void Node::HandleServiceNodeStart(const std::string& key, const std::string& value) {
	LOG_INFO << "Handling service node start for key: " << key << ", value: " << value;
	// Get the service node type from the key prefix
	auto nodeType = NodeSystem::GetServiceTypeFromPrefix(key);

	if (nodeType == kDeployNode) {
		LOG_INFO << "Deploy Service Key: " << key << ", Value: " << value;
		InitializeDeploymentService(value);
	}
	else if (nodeType == kLoginNode) {
		LOG_INFO << "Login Node handling is not yet implemented.";
	}
	else if (eNodeType_IsValid(nodeType)) {
		// Add the service node to the appropriate registry
		AddServiceNode(value, nodeType);
	}
	else {
		LOG_ERROR << "Unknown service type for key: " << key;
	}
}

// Handles the stop of a service node
void Node::HandleServiceNodeStop(const std::string& key, const std::string& value) {
	LOG_INFO << "Handling service node stop for key: " << key << ", value: " << value;
	// Get the service node type from the key prefix
	auto nodeType = NodeSystem::GetServiceTypeFromPrefix(key);

	if (nodeType == kDeployNode) {
		LOG_INFO << "Deploy Service Key: " << key << ", Value: " << value;
	}
	else if (nodeType == kLoginNode) {
		LOG_INFO << "Login Node handling is not yet implemented.";
	}
	else if (eNodeType_IsValid(nodeType)) {
		NodeInfo nodeInfo;

		// Parse the JSON string into NodeInfo protobuf message
		auto result = google::protobuf::util::JsonStringToMessage(value, &nodeInfo);
		if (!result.ok()) {
			LOG_ERROR << "Failed to parse JSON for nodeType: " << nodeType
				<< ", JSON: " << value
				<< ", Error: " << result.message().data();
			return;
		}

		// Remove the node from the registry based on its type
		if (nodeType == kCentreNode) {
			tls.centreNodeRegistry.destroy(entt::entity{ nodeInfo.node_id() });
			LOG_INFO << "Centre node stopped: " << nodeInfo.DebugString();
		}
		else if (nodeType == kSceneNode) {
			tls.sceneNodeRegistry.destroy(entt::entity{ nodeInfo.node_id() });
			LOG_INFO << "Scene node stopped: " << nodeInfo.DebugString();
		}
		else if (nodeType == kGateNode) {
			tls.gateNodeRegistry.destroy(entt::entity{ nodeInfo.node_id() });
			LOG_INFO << "Gate node stopped: " << nodeInfo.DebugString();
		}
	}
	else {
		LOG_ERROR << "Unknown service type for key: " << key;
	}
}

void Node::InitializeGrpcResponseHandlers() {
	// Define handlers for various gRPC response types
	AsyncetcdserverpbKVRangeHandler = [this](const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>& call) {
		if (call->status.ok()) {
			for (const auto& kv : call->reply.kvs()) {
				HandleServiceNodeStart(kv.key(), kv.value());
			}
		}
		else {
			LOG_ERROR << "RPC failed: " << call->status.error_message();
		}
		};

	AsyncetcdserverpbKVPutHandler = [](const std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall>& call) {
		// Handle KV put response if needed
		};

	AsyncetcdserverpbKVDeleteRangeHandler = [](const std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall>& call) {
		// Handle KV delete response if needed
		};

	AsyncetcdserverpbKVTxnHandler = [](const std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall>& call) {
		// Handle KV transaction response if needed
		};

	AsyncetcdserverpbWatchWatchHandler = [this](const ::etcdserverpb::WatchResponse& response) {
		if (response.created()) {
			LOG_TRACE << "Watch created: " << response.created();
			return;
		}

		if (response.canceled()) {
			LOG_INFO << "Watch canceled. Reason: " << response.cancel_reason();
			if (response.compact_revision() > 0) {
				LOG_ERROR << "Revision " << response.compact_revision() << " has been compacted.";
				// Re-initiate watch request with the latest revision
			}
			return;
		}

		for (const auto& event : response.events()) {
			if (event.type() == mvccpb::Event_EventType::Event_EventType_PUT) {
				LOG_INFO << "Key put: " << event.kv().key();
				HandleServiceNodeStart(event.kv().key(), event.kv().value());
			}
			else if (event.type() == mvccpb::Event_EventType::Event_EventType_DELETE) {
				HandleServiceNodeStop(event.kv().key(), event.kv().value());
				LOG_INFO << "Key deleted: " << event.kv().key();
			}
		}
		};
}

void Node::OnConnectedToServer(const OnConnected2TcpServerEvent& es) {
	auto& conn = es.conn_;
	if (!conn->connected()) {
		LOG_INFO << "Client disconnected: " << conn->peerAddress().toIpPort();
		return;
	}

	LOG_INFO << "Successfully connected to server: " << conn->peerAddress().toIpPort();

	// Define a lambda to handle connection logic for different registries
	auto handleConnection = [&](auto& registry, const std::string& registryName, uint32_t messageId, auto onConnectedCallback) {
		for (const auto& [e, client, nodeInfo] : registry.view<RpcClient, NodeInfo>().each()) {
			if (!IsSameAddress(client.peer_addr(), conn->peerAddress())) {
				continue;
			}

			LOG_INFO << "Matched peer address in [" << registryName << "] registry: " << conn->peerAddress().toIpPort();

			RegisterNodeSessionRequest request;
			request.mutable_self_node()->CopyFrom(GetNodeInfo());
			request.mutable_endpoint()->set_ip(conn->localAddress().toIp());
			request.mutable_endpoint()->set_port(conn->localAddress().port());

			// Execute the provided callback
			onConnectedCallback(e, request, client);

			// Trigger a generic event for node connection
			ConnectToNodePbEvent connectToNodePbEvent;
			connectToNodePbEvent.set_entity(entt::to_integral(e));
			connectToNodePbEvent.set_node_type(nodeInfo.node_type());
			tls.dispatcher.trigger(connectToNodePbEvent);

			return true;
		}

		LOG_INFO << "No matching client found in [" << registryName << "] registry for address: " << conn->peerAddress().toIpPort();
		return false;
		};

	// Handle connections for different node types
	handleConnection(
		tls.centreNodeRegistry,
		"CentreNode",
		CentreServiceRegisterNodeSessionMessageId,
		[&](entt::entity e, RegisterNodeSessionRequest& request, RpcClient& client) {
			client.CallRemoteMethod(CentreServiceRegisterNodeSessionMessageId, request);
			OnConnect2CentrePbEvent connect2CentreEvent;
			connect2CentreEvent.set_entity(entt::to_integral(e));
			tls.dispatcher.trigger(connect2CentreEvent);
			LOG_INFO << "Triggered OnConnect2Centre event for entity: " << entt::to_integral(e);

			LOG_INFO << "CentreNode connected. Entity: " << entt::to_integral(e);
		}
	);

	handleConnection(
		tls.sceneNodeRegistry,
		"SceneNode",
		GameServiceRegisterNodeSessionMessageId,
		[&](entt::entity e, RegisterNodeSessionRequest& request, RpcClient& client) {
			client.CallRemoteMethod(GameServiceRegisterNodeSessionMessageId, request);
			LOG_INFO << "SceneNode connected. Entity: " << entt::to_integral(e);
		}
	);

	handleConnection(
		tls.gateNodeRegistry,
		"GateNode",
		GateServiceRegisterNodeSessionMessageId,
		[&](entt::entity e, RegisterNodeSessionRequest& request, RpcClient& client) {
			client.CallRemoteMethod(GateServiceRegisterNodeSessionMessageId, request);
			LOG_INFO << "GateNode connected. Entity: " << entt::to_integral(e);
		}
	);
}


void Node::OnClientConnected(const OnBeConnectedEvent& es) {
	auto& conn = es.conn_;
	if (!conn->connected()) {
		for (const auto& [e, session] : tls.networkRegistry.view<RpcSession>().each()) {
			auto& existingConn = session.connection;
			if (!IsSameAddress(conn->peerAddress(), existingConn->peerAddress())) {
				LOG_TRACE << "Endpoint mismatch: expected IP = " << conn->peerAddress().toIp()
					<< ", port = " << conn->peerAddress().port()
					<< "; actual IP = " << existingConn->peerAddress().toIp()
					<< ", port = " << existingConn->peerAddress().port();
				continue;
			}

			tls.networkRegistry.destroy(e);
			return;
		}
		return;
	}

	auto e = tls.networkRegistry.create();
	tls.networkRegistry.emplace<RpcSession>(e, RpcSession{ conn });

	LOG_INFO << "Client connected: {}" << conn->peerAddress().toIpPort();
}

void Node::HandleNodeRegistration(const RegisterNodeSessionRequest& request) {
	auto& peerNodeInfo = request.self_node();

	LOG_INFO << "Received node registration request:" << request.DebugString();

	auto& serviceNodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GlobalGrpcNodeEntity());

	// Helper lambda to process a registry
	auto processRegistry = [&](auto& registry, const TcpConnectionPtr& conn, const std::string& registryName, const NodeInfoListPBComponent& nodeList) {
		for (const auto& serverNodeInfo : nodeList.node_list()) {
			if (serverNodeInfo.lease_id() != peerNodeInfo.lease_id()) {
				LOG_TRACE << "Mismatch in node type or ID. Expected node: " << serverNodeInfo.DebugString()
					<< "; received node: " << peerNodeInfo.DebugString();
				continue;
			}

			entt::entity id = registry.create(entt::entity{ peerNodeInfo.node_id() });
			if (id != entt::entity{ peerNodeInfo.node_id() })
			{
				LOG_ERROR << "Failed to create node entity: " << entt::to_integral(id) << registryName << " registry.";;
				return false;
			}
			registry.emplace<RpcSession>(id, RpcSession{ conn });
			LOG_INFO << "Node with ID " << peerNodeInfo.node_id() << " found in " << registryName << " registry.";
			return true;

		}
		return false;
		};

	for (const auto& [e, session] : tls.networkRegistry.view<RpcSession>().each()) {
		auto& conn = session.connection;
		if (request.endpoint().ip() != conn->peerAddress().toIp() ||
			request.endpoint().port() != conn->peerAddress().port()) {
			LOG_TRACE << "Endpoint mismatch: expected IP = " << request.endpoint().ip()
				<< ", port = " << request.endpoint().port()
				<< "; actual IP = " << conn->peerAddress().toIp()
				<< ", port = " << conn->peerAddress().port();
			continue;
		}
		// Process each registry using server list type's NodeInfo list
		if (processRegistry(tls.centreNodeRegistry, conn, "CentreNode", serviceNodeList[kCentreNode]) ||
			processRegistry(tls.sceneNodeRegistry, conn, "SceneNode", serviceNodeList[kSceneNode]) ||
			processRegistry(tls.gateNodeRegistry, conn, "GateNode", serviceNodeList[kGateNode])) {
			tls.networkRegistry.destroy(e);
			return;
		}
	}
}