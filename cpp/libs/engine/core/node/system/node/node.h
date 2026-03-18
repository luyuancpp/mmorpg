#pragma once

#include <memory>
#include <muduo/base/AsyncLogging.h>
#include <muduo/net/TcpServer.h>
#include "network/rpc_client.h"
#include "network/rpc_server.h"
#include "proto/common/base/common.pb.h"
#include "time/comp/timer_task_comp.h"
#include "type_define/type_define.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <atomic>
#include <functional>
#include "infra/messaging/kafka/kafka_manager.h"
#include "node/system/etcd/etcd_service.h"
#include "node/system/etcd/etcd_manager.h"
#include "node/system/registration/registration_manager.h"
#include "node/system/discovery/service_discovery_manager.h"
#include "node/system/grpc_channel_cache.h"


enum class NodeIdConflictReason
{
	kLeaseExpiredByEtcd,     // keepalive returned TTL=0
	kLeaseDeadlineExceeded,  // local health monitor: no ACK within TTL
	kReRegistrationFailed,   // re-register CAS failed, another node owns this ID
};

class Node : muduo::noncopyable {
public:
    // 类型定义
    using RpcServerPtr = std::unique_ptr<muduo::net::RpcServer>;
    using ServiceList = std::vector<::google::protobuf::Service*>;
    using CanConnectNodeTypeList = std::set<uint32_t>;
	using ClientList = std::vector<RpcClientPtr>;

    using PartitionClassGuid = std::vector<int32_t>;

    // 构造与析构
    explicit Node(muduo::net::EventLoop* loop, const std::string& logFilePath);
    virtual ~Node();

    // 基本信息获取
    int64_t GetLeaseId() const;
    muduo::net::EventLoop* GetLoop() { return eventLoop; }
    NodeId GetNodeId() const { return GetNodeInfo().node_id(); }
    uint32_t GetNodeType() const { return GetNodeInfo().node_type(); }
    NodeInfo& GetNodeInfo() const;
	KafkaManager& GetKafkaManager() { return kafkaManager; }
    virtual google::protobuf::Service* GetNodeReplyService() { return nullptr; }
    muduo::AsyncLogging& Log() { return logSystem; }
    ClientList& GetZombieClientList() { return zombieClientList; }
    CanConnectNodeTypeList& GetTargetNodeTypeWhitelist() { return targetNodeTypeWhitelist; }
	NodeHandshakeManager& GetNodeRegistrationManager() { return nodeRegistrationManager; }
    ServiceDiscoveryManager& GetServiceDiscoveryManager() { return serviceDiscoveryManager; }
    std::string FormatIpAndPort();
    std::string GetIp();
    uint32_t GetPort();
	EtcdManager& GetEtcdManager() { return etcdManager; }
	grpc_channel_cache::GrpcChannelCache& GetGrpcChannelCache() { return grpcChannelCache; }

    // 节点注册与服务处理
    void HandleServiceNodeStop(const std::string& key, const std::string& nodeJson);

    virtual void StartRpcServer();
    using KafkaMessageHandler = std::function<void(const std::string&, const std::string&)>;
    bool RegisterKafkaMessageHandler(const std::vector<std::string>& topics,
                                     const std::string& groupId,
                                     KafkaMessageHandler handler,
                                     const std::vector<int32_t>& partitions = {});

	// Called before LOG_FATAL when this node's identity is no longer valid.
	// Override in subclasses to flush players, save state, etc.
	// After this returns, the process will terminate.
	virtual void OnNodeIdConflictShutdown(NodeIdConflictReason reason);

	// 工具与状态判断
    bool IsCurrentNode(const NodeInfo& candidateNode) const;
	bool IsServiceStarted() { return rpcServer != nullptr; }
protected:
    // 初始化相关
    void InitRpcServer();
    void Initialize();
    void InitLogSystem();
    void RegisterEventHandlers();
    void LoadConfigs();
    void LoadAllConfigData();
    void SetupTimeZone();
    void InitKafka();
    void StartKafkaPolling();
    virtual bool RegisterKafkaHandlers() { return true; }
    void InitEtcdService();

    void ReleaseNodeId();
    void RegisterHandlers();
    void StopWatchingServiceNodes();
    static void AsyncOutput(const char* msg, int len);
    void StartNodeRegistrationHealthMonitor();

    // 事件处理
    void OnServerConnected(const OnConnected2TcpServerEvent& connectedEvent);

    void Shutdown();
    void ShutdownInLoop();

    // 成员变量
    muduo::net::EventLoop* eventLoop;
    muduo::AsyncLogging logSystem;
    RpcServerPtr rpcServer;
    TimerTaskComp grpcHandlerTimer;
    TimerTaskComp serviceHealthMonitorTimer;
    TimerTaskComp acquireNodeTimer;
	TimerTaskComp acquirePortTimer;
	TimerTaskComp kafkaProducerTimer;
	TimerTaskComp kafkaConsumerTimer;
    CanConnectNodeTypeList targetNodeTypeWhitelist;
    ClientList zombieClientList;
    std::unordered_map<std::string,int64_t> revision;
    bool hasSentRange{ false };
    bool hasSentWatch{ false };
	boost::uuids::random_generator gen;
	KafkaManager kafkaManager;
    EtcdManager etcdManager;
    NodeHandshakeManager nodeRegistrationManager;
    ServiceDiscoveryManager serviceDiscoveryManager;
    grpc_channel_cache::GrpcChannelCache grpcChannelCache;
    std::atomic<bool> shutdownStarted{ false };
    bool kafkaPollingStarted{ false };
};

extern Node* gNode;

