#pragma once

#include <memory>
#include <muduo/base/AsyncLogging.h>
#include <muduo/net/TcpServer.h>
#include "network/rpc_client.h"
#include "network/rpc_server.h"
#include "proto/common/common.pb.h"
#include "time/comp/timer_task_comp.h"
#include "type_define/type_define.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "kafka_manager.h"
#include "etcd_service.h"
#include "etcd_manager.h"
#include "registration_manager.h"
#include "service_discovery_manager.h"

class RegisterNodeSessionRequest;
class RegisterNodeSessionResponse;

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
    muduo::net::EventLoop* GetLoop() { return eventLoop; }
    NodeId GetNodeId() const { return GetNodeInfo().node_id(); }
    uint32_t GetNodeType() const { return GetNodeInfo().node_type(); }
    NodeInfo& GetNodeInfo() const;
	KafkaManager& GetKafkaManager() { return kafkaManager; }
	int64_t GetLeaseId() const { return leaseId; }
    virtual ::google::protobuf::Service* GetNodeReplyService() { return {}; }
    inline [[nodiscard]] muduo::AsyncLogging& Log() { return logSystem; }
    [[nodiscard]] RpcClientPtr& GetZoneCentreNode() { return zoneCentreNode; }
	void SetZoneCentreNode(RpcClientPtr& c) { zoneCentreNode = c; }
    ClientList& GetZombieClientList() { return zombieClientList; }
    CanConnectNodeTypeList& GetTargetNodeTypeWhitelist() { return targetNodeTypeWhitelist; }
	NodeRegistrationManager& GetNodeRegistrationManager() { return nodeRegistrationManager; }
    ServiceDiscoveryManager& GetServiceDiscoveryManager() { return serviceDiscoveryManager; }
    std::string FormatIpAndPort();
    std::string GetIp();
    uint32_t GetPort();
	EtcdManager& GetEtcdManager() { return etcdManager; }

    void CallRemoteMethodZoneCenter(uint32_t message_id, const ::google::protobuf::Message& request);

    // 节点注册与服务处理
    void HandleServiceNodeStop(const std::string& key, const std::string& value);

    virtual void StartRpcServer();
	
	// 工具与状态判断
	bool IsMyNode(const NodeInfo& node) const;
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
    void InitEtcdService();

    void ReleaseNodeId();
    void RegisterHandlers();
    void StopWatchingServiceNodes();
    static void AsyncOutput(const char* msg, int len);
    void StartServiceHealthMonitor();

    // 事件处理
    void OnServerConnected(const OnConnected2TcpServerEvent& es);
    void OnClientConnected(const OnTcpClientConnectedEvent& es);



    void Shutdown();

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
    RpcClientPtr zoneCentreNode;
    CanConnectNodeTypeList targetNodeTypeWhitelist;
    ClientList zombieClientList;
    std::unordered_map<std::string,int64_t> revision;
    bool hasSentRange{ false };
    bool hasSentWatch{ false };
	int64_t leaseId{ 0 };
	boost::uuids::random_generator gen;
	KafkaManager kafkaManager;
    EtcdManager etcdManager;
    NodeRegistrationManager nodeRegistrationManager;
    ServiceDiscoveryManager serviceDiscoveryManager;
};

extern Node* gNode;

