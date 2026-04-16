#pragma once

#include <memory>
#include <muduo/base/AsyncLogging.h>
#include <muduo/net/TcpServer.h>
#include "network/rpc_client.h"
#include "network/rpc_server.h"
#include "node/system/node/node_util.h"
#include "time/comp/timer_task_comp.h"
#include "type_define/type_define.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <atomic>
#include <functional>
#include <thread_context/registry_manager.h>
#include <thread_context/entity_manager.h>
#include <network/node_utils.h>
#include "infra/messaging/kafka/kafka_manager.h"
#include "node/system/etcd/etcd_service.h"
#include "node/system/etcd/etcd_manager.h"
#include "node/system/registration/registration_manager.h"
#include "node/system/discovery/service_discovery_manager.h"
#include "node/system/grpc_channel_cache.h"
#include <grpcpp/grpcpp.h>
#include <thread>

// Tracks a pending node removal that can be cancelled if the node re-registers.
struct PendingNodeRemoval
{
    TimerTaskComp timer;
    NodeInfo nodeInfo;
};

enum class NodeIdConflictReason
{
    kLeaseExpiredByEtcd,    // keepalive returned TTL=0
    kLeaseDeadlineExceeded, // local health monitor: no ACK within TTL
    kReRegistrationFailed,  // re-register CAS failed, another node owns this ID
};

class Node : muduo::noncopyable
{
public:
    using RpcServerPtr = std::unique_ptr<muduo::net::RpcServer>;
    using ServiceList = std::vector<::google::protobuf::Service *>;
    using CanConnectNodeTypeList = std::set<uint32_t>;
    using ClientList = std::vector<RpcClientPtr>;

    using PartitionClassIds = std::vector<int32_t>;

    using AfterStartFn = std::function<void(Node &)>;
    using KafkaHandlersFn = std::function<bool(Node &)>;
    using BeforeShutdownFn = std::function<void(Node &)>;
    using OnConflictShutdownFn = std::function<void(Node &, NodeIdConflictReason)>;

    explicit Node(muduo::net::EventLoop *loop, const std::string &logFilePath);

    // Full construction: derives log dir from nodeType (e.g. GateNodeService -> "logs/gate").
    // Calls Initialize() and registers thread observability.
    Node(muduo::net::EventLoop *loop,
         uint32_t nodeType,
         CanConnectNodeTypeList connectTo,
         ::google::protobuf::Service *replyService = nullptr);

    virtual ~Node();

    // Basic information accessors
    int64_t GetLeaseId() const;
    muduo::net::EventLoop *GetLoop() { return eventLoop; }
    NodeId GetNodeId() const { return GetNodeInfo().node_id(); }
    uint32_t GetNodeType() const { return GetNodeInfo().node_type(); }
    NodeInfo &GetNodeInfo() const;
    KafkaManager &GetKafkaManager() { return kafkaManager; }
    virtual google::protobuf::Service *GetNodeReplyService() { return replyService_; }
    muduo::AsyncLogging &Log() { return logSystem; }
    ClientList &GetDisconnectedClientList() { return disconnectedClientList; }
    CanConnectNodeTypeList &GetTargetNodeTypeWhitelist() { return targetNodeTypeWhitelist; }
    NodeHandshakeManager &GetNodeRegistrationManager() { return nodeRegistrationManager; }
    ServiceDiscoveryManager &GetServiceDiscoveryManager() { return serviceDiscoveryManager; }
    EtcdManager &GetEtcdManager() { return etcdManager; }
    grpc_channel_cache::GrpcChannelCache &GetGrpcChannelCache() { return grpcChannelCache; }

    // Only valid after StartRpcServer().
    muduo::net::TcpServer &GetTcpServer() { return rpcServer->GetTcpServer(); }

    // Go-style: register hooks before start instead of subclassing.
    Node &SetAfterStart(AfterStartFn fn)
    {
        afterStartFn_ = std::move(fn);
        return *this;
    }
    Node &SetKafkaHandlers(KafkaHandlersFn fn)
    {
        kafkaHandlersFn_ = std::move(fn);
        return *this;
    }
    Node &SetBeforeShutdown(BeforeShutdownFn fn)
    {
        beforeShutdownFn_ = std::move(fn);
        return *this;
    }
    Node &SetOnConflictShutdown(OnConflictShutdownFn fn)
    {
        onConflictShutdownFn_ = std::move(fn);
        return *this;
    }
    bool HasDiscoveredServiceNode(uint32_t nodeType) const
    {
        auto &allNodes = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());
        if (nodeType >= allNodes.size())
        {
            return false;
        }

        const auto &nodes = allNodes[nodeType].node_list();
        return nodes.size() > 0;
    }

    std::vector<uint32_t> CollectMissingDiscoveredServiceNodes(const std::vector<uint32_t> &requiredNodeTypes) const
    {
        std::vector<uint32_t> missingNodeTypes;
        missingNodeTypes.reserve(requiredNodeTypes.size());

        for (uint32_t nodeType : requiredNodeTypes)
        {
            if (!HasDiscoveredServiceNode(nodeType))
            {
                missingNodeTypes.push_back(nodeType);
            }
        }

        return missingNodeTypes;
    }

    static std::string FormatNodeTypeNames(const std::vector<uint32_t> &nodeTypes)
    {
        std::string output;
        for (size_t i = 0; i < nodeTypes.size(); ++i)
        {
            if (i > 0)
                output += ",";
            output += eNodeType_Name(static_cast<eNodeType>(nodeTypes[i]));
        }
        return output;
    }

    // Node registration and service handling
    void HandleServiceNodeStop(const std::string &key, const std::string &nodeJson);
    void CancelPendingNodeRemoval(const std::string &nodeUuid);

    virtual void StartRpcServer();
    using KafkaMessageHandler = std::function<void(const std::string &, const std::string &)>;
    bool RegisterKafkaMessageHandler(const std::vector<std::string> &topics,
                                     const std::string &groupId,
                                     KafkaMessageHandler handler,
                                     const std::vector<int32_t> &partitions = {});

    // gRPC server: register a service before StartRpcServer().
    void RegisterGrpcService(grpc::Service *service);
    const std::vector<grpc::Service *> &GetGrpcServices() const { return grpcServices_; }

    // Called before LOG_FATAL when this node's identity is no longer valid.
    // Override in subclasses to flush players, save state, etc.
    // After this returns, the process will terminate.
    virtual void OnNodeIdConflictShutdown(NodeIdConflictReason reason);

    // Graceful shutdown (public so OS signal handlers can invoke it).
    void Shutdown();

    // Utility and state queries
    bool IsCurrentNode(const NodeInfo &candidateNode) const;
    bool IsServiceStarted() { return rpcServer != nullptr; }

protected:
    // Initialization
    void InitRpcServer();
    void Initialize();
    void InitLogSystem();
    void RegisterEventHandlers();
    void LoadConfigs();
    void LoadAllConfigData();
    void SetupTimeZone();
    void InitKafka();
    void StartKafkaPolling();
    virtual bool RegisterKafkaHandlers() { return kafkaHandlersFn_ ? kafkaHandlersFn_(*this) : true; }
    void InitEtcdService();

    // gRPC server lifecycle (called automatically if services registered).
    void StartGrpcServer();
    void ShutdownGrpcServer();

    void ReleaseNodeId();
    void RegisterHandlers();
    void StopWatchingServiceNodes();
    static void AsyncOutput(const char *msg, int len);
    void StartNodeRegistrationHealthMonitor();
    void ExecuteNodeRemoval(const NodeInfo &stoppedNode);

    // Event handling
    void OnServerConnected(const OnConnected2TcpServerEvent &connectedEvent);

    void ShutdownInLoop();

    // Member variables
    muduo::net::EventLoop *eventLoop;
    muduo::AsyncLogging logSystem;
    RpcServerPtr rpcServer;
    TimerTaskComp grpcHandlerTimer;
    TimerTaskComp serviceHealthMonitorTimer;
    TimerTaskComp acquireNodeTimer;
    TimerTaskComp acquirePortTimer;
    TimerTaskComp kafkaProducerTimer;
    TimerTaskComp kafkaConsumerTimer;
    CanConnectNodeTypeList targetNodeTypeWhitelist;
    ClientList disconnectedClientList;
    std::unordered_map<std::string, int64_t> revision;
    bool hasSentRange{false};
    bool hasSentWatch{false};
    boost::uuids::random_generator uuidGenerator;
    KafkaManager kafkaManager;
    EtcdManager etcdManager;
    NodeHandshakeManager nodeRegistrationManager;
    ServiceDiscoveryManager serviceDiscoveryManager;
    grpc_channel_cache::GrpcChannelCache grpcChannelCache;
    std::atomic<bool> shutdownStarted{false};
    bool kafkaPollingStarted{false};

    // Grace-period pending removals: node_uuid -> pending removal state.
    // When etcd fires DELETE, removal is deferred; a subsequent PUT cancels it.
    std::unordered_map<std::string, std::unique_ptr<PendingNodeRemoval>> pendingNodeRemovals_;

    ::google::protobuf::Service *replyService_{nullptr};
    AfterStartFn afterStartFn_;
    KafkaHandlersFn kafkaHandlersFn_;
    BeforeShutdownFn beforeShutdownFn_;
    OnConflictShutdownFn onConflictShutdownFn_;

    // gRPC server (optional, started only when services are registered via RegisterGrpcService).
    std::vector<grpc::Service *> grpcServices_;
    std::unique_ptr<grpc::Server> grpcServer_;
    std::thread grpcServerThread_;
};

// DependencyGate — polls for required service-discovery dependencies before
// firing a one-shot "ready" callback.  Eliminates boilerplate in node main files.
//
// Usage (inside SetAfterStart):
//   DependencyGate gate;
//   gate.WaitAndRun(n, { SceneManagerNodeService }, [&](auto&) {
//       worldTimer.RunEvery(dt, World::Update);
//   });
//
struct DependencyGate
{
    TimerTaskComp probeTimer;
    bool ready{false};
    uint32_t waitLogTick{0};

    // nodeName: e.g. "Scene", "Gate" — used only in log messages.
    // requiredNodeTypes: service types to wait for.
    // onReady: invoked exactly once when all dependencies are discovered.
    template <typename TNode, typename ReadyFn>
    void WaitAndRun(TNode &node,
                    const std::vector<uint32_t> &requiredNodeTypes,
                    ReadyFn onReady,
                    const std::string &nodeName = "",
                    double probeIntervalSec = 1.0,
                    uint32_t logEveryNTicks = 5)
    {
        probeTimer.RunEvery(probeIntervalSec,
                            [this, &node, requiredNodeTypes, onReady = std::move(onReady), nodeName, logEveryNTicks]
                            {
                                if (ready)
                                    return;

                                auto missing = node.CollectMissingDiscoveredServiceNodes(requiredNodeTypes);
                                if (!missing.empty())
                                {
                                    if (++waitLogTick % logEveryNTicks == 0)
                                    {
                                        LOG_INFO << nodeName << " startup waiting dependencies: missing="
                                                 << Node::FormatNodeTypeNames(missing);
                                    }
                                    return;
                                }

                                ready = true;
                                LOG_INFO << nodeName << " dependency ready: all required nodes discovered. "
                                         << "required=" << Node::FormatNodeTypeNames(requiredNodeTypes);
                                onReady(node);
                            });
    }
};

extern Node *gNode;
