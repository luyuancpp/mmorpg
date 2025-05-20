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


// 重构思路（伪代码）：
// 1. 统一命名风格：类成员变量用  前缀，局部变量驼峰命名，函数名大写每个单词。
// 2. 拆分过长函数，合并重复逻辑，简化流程。
// 3. 明确职责：配置加载、日志、RPC、gRPC、etcd、节点注册、连接、事件处理分块。
// 4. 变量名更直观，注释更清晰。
// 5. 保持 node 外部接口不变。
// 6. 只重构 Node 类本身。

Node::Node(muduo::net::EventLoop* eventLoop, const std::string& logFilePath)
    : eventLoop(eventLoop),
    logSystem(logFilePath, kMaxLogFileRollSize, 1) {
    LOG_INFO << "Node created, log file: " << logFilePath;
}

Node::~Node() {
    LOG_INFO << "Node destroyed.";
    Shutdown();
}

NodeInfo& Node::GetNodeInfo() const {
    return tls.globalRegistry.get_or_emplace<NodeInfo>(GlobalEntity());
}

std::string Node::GetServiceName(uint32_t nodeType) const {
    return eNodeType_Name(nodeType) + ".rpc";
}

void Node::Initialize() {
    LOG_TRACE << "Node initializing...";

    RegisterEventHandlers();
    LoadConfigs();
    InitRpcServer();
    InitLogSystem();
    SetupTimeZone();
    LoadAllConfigData();
    InitGrpcClients();
    InitGrpcQueues();
    RegisterGrpcHandlers();
    RequestEtcdLease();

    LOG_TRACE << "Node initialization complete.";
}

void Node::InitRpcServer() {
    NodeInfo& nodeInfo = GetNodeInfo();
    nodeInfo.mutable_endpoint()->set_ip(localip());
    nodeInfo.mutable_endpoint()->set_port(get_available_port(GetNodeType() * 10000));
    nodeInfo.set_node_type(GetNodeType());
    nodeInfo.set_scene_node_type(tlsCommonLogic.GetGameConfig().scene_node_type());
    nodeInfo.set_protocol_type(PROTOCOL_TCP);
    nodeInfo.set_launch_time(TimeUtil::NowSecondsUTC());
    nodeInfo.set_zone_id(tlsCommonLogic.GetGameConfig().zone_id());

    InetAddress rpcAddr(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port());
    rpcServer = std::make_unique<RpcServerPtr::element_type>(eventLoop, rpcAddr);
    rpcServer->start();

    LOG_INFO << "RPC server started at " << rpcAddr.toIpPort();
}

void Node::StartRpcServer() {
    FetchServiceNodes();
    StartWatchingServiceNodes();
    tls.dispatcher.trigger<OnServerStart>();
    LOG_TRACE << "RPC server started: " << GetNodeInfo().DebugString();
}

void Node::Shutdown() {
    LOG_TRACE << "Node shutting down...";
    StopWatchingServiceNodes();
    tls.Clear();
    logSystem.stop();
    ReleaseNodeId();
    renewLeaseTimer.Cancel();
    etcdQueueTimer.Cancel();
    LOG_TRACE << "Node shutdown complete.";
    tls.Clear();
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
    LoadAllConfig();
    LoadAllConfigAsyncWhenServerLaunch();
    OnConfigLoadSuccessful();
}

void Node::SetupTimeZone() {
    const muduo::TimeZone hkTz("zoneinfo/Asia/Hong_Kong");
    muduo::Logger::setTimeZone(hkTz);
}

void Node::InitGrpcClients() {
    GrpcClientSystem::InitEtcdStubs(tlsCommonLogic.GetBaseDeployConfig().etcd_hosts());
}

void Node::InitGrpcQueues() {
    InitetcdserverpbKVCompletedQueue(tls.globalNodeRegistry, GetGlobalGrpcNodeEntity());
    InitetcdserverpbWatchCompletedQueue(tls.globalNodeRegistry, GetGlobalGrpcNodeEntity());
    InitetcdserverpbLeaseCompletedQueue(tls.globalNodeRegistry, GetGlobalGrpcNodeEntity());

    etcdQueueTimer.RunEvery(0.001, [] {
        HandleetcdserverpbKVCompletedQueueMessage(tls.globalNodeRegistry);
        HandleetcdserverpbWatchCompletedQueueMessage(tls.globalNodeRegistry);
        HandleetcdserverpbLeaseCompletedQueueMessage(tls.globalNodeRegistry);
    });
}

std::string Node::MakeEtcdKey(const NodeInfo& nodeInfo) {
    return GetServiceName(nodeInfo.node_type()) +
           "/zone/" + std::to_string(nodeInfo.zone_id()) +
           "/node_type/" + std::to_string(nodeInfo.node_type()) +
           "/node_id/" + std::to_string(nodeInfo.node_id());
}

void Node::RegisterSelf() {
    EtcdHelper::PutServiceNodeInfo(GetNodeInfo(), MakeEtcdKey(GetNodeInfo()));
}

void Node::FetchServiceNodes() {
    for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
        EtcdHelper::RangeQuery(prefix);
    }
}

void Node::StartWatchingServiceNodes() {
    for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
        EtcdHelper::StartWatchingPrefix(prefix);
    }
}

void Node::StopWatchingServiceNodes() {
    EtcdHelper::StopAllWatching();
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
        LOG_ERROR << "Unsupported protocol: " << nodeInfo.protocol_type()
                  << " node: " << nodeInfo.DebugString();
        break;
    }
}

void Node::ConnectToGrpcNode(const NodeInfo& nodeInfo) {
    auto& nodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
    auto& registry = NodeSystem::GetRegistryForNodeType(nodeInfo.node_type());

    const entt::entity entityId{ nodeInfo.node_id() };
    auto createdId = registry.create(entityId);
    if (createdId != entityId) {
        LOG_ERROR << "Login node not found: " << entt::to_integral(createdId);
        return;
    }

    registry.emplace<std::shared_ptr<grpc::Channel>>(entityId,
        grpc::CreateChannel(::FormatIpAndPort(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port()),
                            grpc::InsecureChannelCredentials()));
    ProcessGrpcNode(nodeInfo);
}

void Node::ConnectToTcpNode(const NodeInfo& nodeInfo) {
    auto& registry = NodeSystem::GetRegistryForNodeType(nodeInfo.node_type());
    entt::entity entityId{ nodeInfo.node_id() };

    if (registry.valid(entityId)) {
        if (auto* existInfo = registry.try_get<NodeInfo>(entityId);
            existInfo && existInfo->node_id() == nodeInfo.node_id()) {
            LOG_TRACE << "Node exists, skip: " << nodeInfo.node_id();
            return;
        } else {
            LOG_TRACE << "Node exists but info mismatch: " << nodeInfo.node_id();
            return;
        }
    }

    const auto createdId = registry.create(entityId);
    if (createdId != entityId) {
        LOG_ERROR << "Create node entity failed: " << entt::to_integral(createdId);
        return;
    }

    InetAddress endpoint(nodeInfo.endpoint().ip(), nodeInfo.endpoint().port());
    auto& client = registry.emplace<RpcClient>(createdId, eventLoop, endpoint);
    registry.emplace<NodeInfo>(entityId, nodeInfo);
    client.registerService(GetNodeReplyService());
    client.connect();

    if (nodeInfo.node_type() == CentreNodeService &&
        nodeInfo.zone_id() == tlsCommonLogic.GetGameConfig().zone_id()) {
        zoneCentreNode = &client;
    }
}

void Node::ConnectToHttpNode(const NodeInfo& nodeInfo) {
    // HTTP连接逻辑可扩展
}

void Node::ReleaseNodeId() {
    EtcdHelper::RevokeLeaseAndCleanup(static_cast<int64_t>(GetNodeInfo().lease_id()));
}

void InitRepliedHandler();
void Node::RegisterGrpcHandlers() {
    InitMessageInfo();
    InitGrpcResponseHandlers();
    InitRepliedHandler();
}

void Node::AsyncOutput(const char* msg, int len) {
    logger().append(msg, len);
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

void Node::AddServiceNode(const std::string& nodeJson, uint32_t nodeType) {
    LOG_INFO << "Add service node type " << nodeType << " JSON: " << nodeJson;

    if (!eNodeType_IsValid(static_cast<int32_t>(nodeType))) {
        LOG_ERROR << "Invalid node type: " << nodeType;
        return;
    }

    NodeInfo newNodeInfo;
    auto parseResult = google::protobuf::util::JsonStringToMessage(nodeJson, &newNodeInfo);
    if (!parseResult.ok()) {
        LOG_ERROR << "Parse node JSON failed, type: " << nodeType
                  << ", JSON: " << nodeJson
                  << ", Error: " << parseResult.message().data();
        return;
    }

    auto& nodeRegistry = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
    auto& nodeList = *nodeRegistry[nodeType].mutable_node_list();

    for (const auto& existNode : nodeList) {
        if (existNode.lease_id() == newNodeInfo.lease_id()) {
            LOG_INFO << "Node exists, IP: " << existNode.endpoint().ip()
                     << ", Port: " << existNode.endpoint().port();
            return;
        }
    }

    *nodeList.Add() = newNodeInfo;
    LOG_INFO << "Node added, type: " << nodeType << ", info: " << newNodeInfo.DebugString();

    if (!targetNodeTypeWhitelist.contains(nodeType)) return;

    auto isRegistered = [&]() -> bool {
        entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);
        for (const auto& [entity, client, nodeInfo] : registry.view<RpcClient, NodeInfo>().each()) {
            if (nodeInfo.endpoint().ip() == newNodeInfo.endpoint().ip() &&
                nodeInfo.endpoint().port() == newNodeInfo.endpoint().port()) {
                LOG_INFO << "Node already registered, IP: " << nodeInfo.endpoint().ip()
                         << ", Port: " << nodeInfo.endpoint().port();
                return true;
            }
        }
        return false;
    };

    if (isRegistered()) return;

    ConnectToNode(newNodeInfo);
    LOG_INFO << "Connected to node: " << newNodeInfo.DebugString();
}

inline bool IsTcpNodeType(int nodeType) {
    static const std::unordered_set<int> validTypes = {
        CentreNodeService,
        SceneNodeService,
        GateNodeService
    };
    return validTypes.contains(nodeType);
}

void Node::HandleServiceNodeStart(const std::string& key, const std::string& value) {
    LOG_DEBUG << "Service node start, key: " << key << ", value: " << value;
    if (const auto nodeType = NodeSystem::GetServiceTypeFromPrefix(key); eNodeType_IsValid(nodeType)) {
        AddServiceNode(value, nodeType);
    } else {
        LOG_ERROR << "Unknown service type for key: " << key;
    }
}

void Node::HandleServiceNodeStop(const std::string& key, const std::string& value) {
    LOG_DEBUG << "Service node stop, key: " << key << ", value: " << value;
    const auto nodeType = NodeSystem::GetServiceTypeFromPrefix(key);
    if (!eNodeType_IsValid(nodeType)) {
        LOG_TRACE << "Unknown service type for key: " << key;
        return;
    }

    std::regex pattern(R"(.*?/node_type/(\d+)/node_id/(\d+))");
    std::smatch match;
    if (!std::regex_match(key, match, pattern)) return;

    uint32_t nodeId = std::stoul(match[2]);
    if (nodeId > std::numeric_limits<uint32_t>::max()) {
        LOG_ERROR << "NodeId exceeds uint32_t.";
        return;
    }

    auto& nodeRegistry = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
    auto& nodeList = *nodeRegistry[nodeType].mutable_node_list();
    for (auto it = nodeList.begin(); it != nodeList.end(); ) {
        if (it->node_type() == nodeType && it->node_id() == nodeId) {
            LOG_INFO << "Remove node lease_id: " << it->lease_id();
            it = nodeList.erase(it);
            break;
        } else {
            ++it;
        }
    }

    ProcessNodeStop(nodeType, nodeId);
    entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);
    Destroy(registry, entt::entity{nodeId});
    LOG_INFO << "Service node stopped, id: " << nodeId;
}

void Node::InitGrpcResponseHandlers() {
    AsyncetcdserverpbKVRangeHandler = [this](const std::unique_ptr<AsyncetcdserverpbKVRangeGrpcClientCall>& call) {
        if (call->status.ok()) {
            for (const auto& kv : call->reply.kvs()) {
                HandleServiceNodeStart(kv.key(), kv.value());
            }
        } else {
            LOG_ERROR << "KVRange RPC failed: " << call->status.error_message();
        }
    };

    AsyncetcdserverpbKVPutHandler = [this](const std::unique_ptr<AsyncetcdserverpbKVPutGrpcClientCall>& call) {
        LOG_DEBUG << "Put response: " << call->reply.DebugString();
        StartWatchingServiceNodes();
    };

    AsyncetcdserverpbKVDeleteRangeHandler = [](const std::unique_ptr<AsyncetcdserverpbKVDeleteRangeGrpcClientCall>&) {};

    AsyncetcdserverpbKVTxnHandler = [this](const std::unique_ptr<AsyncetcdserverpbKVTxnGrpcClientCall>& call) {
        if (call->status.ok()) {
            LOG_DEBUG << "Txn response: " << call->reply.DebugString();
            call->reply.succeeded() ? StartRpcServer() : AcquireNode();
        } else {
            LOG_ERROR << "KVTxn RPC failed: " << call->status.error_message();
        }
    };

    AsyncetcdserverpbWatchWatchHandler = [this](const ::etcdserverpb::WatchResponse& response) {
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
            } else if (event.type() == mvccpb::Event_EventType::Event_EventType_DELETE) {
                HandleServiceNodeStop(event.kv().key(), event.kv().value());
                LOG_INFO << "Key deleted: " << event.kv().key();
            }
        }
    };

    AsyncetcdserverpbLeaseLeaseGrantHandler = [this](const std::unique_ptr<AsyncetcdserverpbLeaseLeaseGrantGrpcClientCall>& call) {
        if (call->status.ok()) {
            GetNodeInfo().set_lease_id(call->reply.id());
            KeepNodeAlive();
            AcquireNode();
            LOG_INFO << "Lease granted: " << call->reply.DebugString();
        } else {
            LOG_ERROR << "LeaseGrant RPC failed: " << call->status.error_message();
        }
    };
}

void Node::OnServerConnected(const OnConnected2TcpServerEvent& event) {
    auto& conn = event.conn_;
    if (!conn->connected()) {
        LOG_INFO << "Client disconnected: " << conn->peerAddress().toIpPort();
        return;
    }
    LOG_INFO << "Connected to server: " << conn->peerAddress().toIpPort();
    RegisterNodeSessions(conn);
}

static uint32_t kNodeTypeToMessageId[eNodeType_MAX] = {
    0,
    0,
    CentreServiceRegisterNodeSessionMessageId,
    GameServiceRegisterNodeSessionMessageId,
    GateServiceRegisterNodeSessionMessageId
};

void Node::RegisterNodeSessions(const muduo::net::TcpConnectionPtr& conn) {
    TryRegisterNodeSession(CentreNodeService, conn);
    TryRegisterNodeSession(SceneNodeService, conn);
    TryRegisterNodeSession(GateNodeService, conn);
}

void Node::TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const {
    entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);
    for (const auto& [entity, client, nodeInfo] : registry.view<RpcClient, NodeInfo>().each()) {
        if (!IsSameAddress(client.peer_addr(), conn->peerAddress())) continue;
        LOG_INFO << "Peer address match in " << tls.GetRegistryName(registry)
                 << ": " << conn->peerAddress().toIpPort();
        registry.emplace<TimerTaskComp>(entity).RunAfter(0.5, [conn, this, nodeType, &client]() {
            RegisterNodeSessionRequest req;
            req.mutable_self_node()->CopyFrom(GetNodeInfo());
            req.mutable_endpoint()->set_ip(conn->localAddress().toIp());
            req.mutable_endpoint()->set_port(conn->localAddress().port());
            client.CallRemoteMethod(kNodeTypeToMessageId[nodeType], req);
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
    auto& peerNodeInfo = request.self_node();
    response.mutable_peer_node()->CopyFrom(GetNodeInfo());
    LOG_TRACE << "Node registration request: " << request.DebugString();

    auto tryRegister = [&](const TcpConnectionPtr& conn, uint32_t nodeType) -> bool {
        const auto& nodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
        for (auto& serverNodeInfo : nodeList[nodeType].node_list()) {
            if (serverNodeInfo.lease_id() != peerNodeInfo.lease_id()) continue;
            entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);
            entt::entity entity = registry.create(entt::entity{ peerNodeInfo.node_id() });
            if (entity != entt::entity{ peerNodeInfo.node_id() }) {
                LOG_ERROR << "Create node entity failed in " << tls.GetRegistryName(registry);
                return false;
            }
            registry.emplace<RpcSession>(entity, RpcSession{ conn });
            LOG_INFO << "Node registered, id: " << peerNodeInfo.node_id()
                     << " in " << tls.GetRegistryName(registry);
            return true;
        }
        return false;
    };

    for (const auto& [entity, session] : tls.sessionRegistry.view<RpcSession>().each()) {
        auto& conn = session.connection;
        if (!IsSameAddress(conn->peerAddress(), muduo::net::InetAddress(request.endpoint().ip(), request.endpoint().port()))) continue;
        for (uint32_t nodeType = eNodeType_MIN; nodeType < eNodeType_MAX; ++nodeType) {
            if (GetNodeType() == nodeType || !IsTcpNodeType(nodeType)) continue;
            if (tryRegister(conn, nodeType)) {
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
            LOG_INFO << "CentreNode connected, entity: " << entt::to_integral(entity);
        }
        registry.remove<TimerTaskComp>(entity);
        break;
    }
}

void Node::HandleNodeRegistrationResponse(const RegisterNodeSessionResponse& response) const {
    LOG_INFO << "Node registration response: " << response.DebugString();
    uint32_t nodeType = response.peer_node().node_type();
    entt::registry& registry = NodeSystem::GetRegistryForNodeType(nodeType);
    if (response.error_message().id() != kCommon_errorOK) {
        LOG_TRACE << "Registration failed: " << response.DebugString();
        for (const auto& [entity, client, nodeInfo] : registry.view<RpcClient, NodeInfo>().each()) {
            if (!IsSameAddress(client.peer_addr(), muduo::net::InetAddress(
                response.peer_node().endpoint().ip(),
                response.peer_node().endpoint().port()))) continue;
            registry.get<TimerTaskComp>(entity).RunAfter(0.5, [this, &client, nodeType]() {
                RegisterNodeSessionRequest req;
                *req.mutable_self_node() = GetNodeInfo();
                req.mutable_endpoint()->set_ip(client.local_addr().toIp());
                req.mutable_endpoint()->set_port(client.local_addr().port());
                client.CallRemoteMethod(kNodeTypeToMessageId[nodeType], req);
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

void Node::AcquireNode() {
    auto& nodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity())[GetNodeType()];
    auto& protoList = *nodeList.mutable_node_list();
    uint32_t maxNodeId = 0;
    UInt32Set usedIds;
    for (const auto& node : protoList) {
        maxNodeId = std::max(maxNodeId, node.node_id());
        usedIds.insert(node.node_id());
    }
    constexpr uint32_t randomOffset = 5;
    uint32_t searchLimit = maxNodeId + tlsCommonLogic.GetRng().Rand<uint32_t>(0, randomOffset);
    uint32_t assignedId = 0;
    for (uint32_t id = 0; id < searchLimit; ++id) {
        if (!usedIds.contains(id)) {
            assignedId = id;
            break;
        }
    }
    GetNodeInfo().set_node_id(assignedId);
    const auto serviceKey = MakeEtcdKey(GetNodeInfo());
    EtcdHelper::PutIfAbsent(serviceKey, GetNodeInfo());
}

void Node::KeepNodeAlive() {
    renewLeaseTimer.RunEvery(tlsCommonLogic.GetBaseDeployConfig().lease_renew_interval(), [this]() {
        etcdserverpb::LeaseKeepAliveRequest req;
        req.set_id(static_cast<int64_t>(GetNodeInfo().lease_id()));
        SendetcdserverpbLeaseLeaseKeepAlive(tls.globalNodeRegistry, GetGlobalGrpcNodeEntity(), req);
    });
}

void Node::RequestEtcdLease() {
    uint64_t ttlSeconds = tlsCommonLogic.GetBaseDeployConfig().node_ttl_seconds();
    EtcdHelper::GrantLease(ttlSeconds);
}

