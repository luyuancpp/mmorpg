#pragma once

#include <memory>
#include <muduo/base/AsyncLogging.h>
#include <muduo/net/TcpServer.h>
#include "grpc/generator/proto/common/deploy_service_grpc.h"
#include "network/rpc_client.h"
#include "network/rpc_server.h"
#include "proto/common/common.pb.h"
#include "time/comp/timer_task_comp.h"
#include "type_define/type_define.h"

class Node : muduo::noncopyable{
public:
    using RpcServerPtr = std::unique_ptr<muduo::net::RpcServer>;
    using ServiceList = std::vector<::google::protobuf::Service*>;

    explicit Node(muduo::net::EventLoop* loop, const std::string& logFilePath);
    virtual ~Node();

    virtual NodeId GetNodeId() final { return GetNodeInfo().node_id(); }
    virtual uint32_t GetNodeType() const = 0;
    virtual NodeInfo& GetNodeInfo() = 0;
	virtual std::string GetServiceName() const = 0;
	virtual ::google::protobuf::Service* GetNodeRepleyService() { return {}; }
    inline [[nodiscard]] muduo::AsyncLogging& Log() { return muduoLog; }
	[[nodiscard]] RpcClientPtr& GetZoneCentreNode() { return zoneCentreNode; }
    std::string FormatIpAndPort() ;
	std::string GetIp();
	uint32_t GetPort();

    void InitDeployService(const std::string& service_address);
    void OnStartServiceNode(const std::string& key, const std::string& value);
	void OnStopServiceNode(const std::string& key, const std::string& value);


protected:
    virtual void Initialize();
    void SetupRpcServer ();
    virtual void StartRpcServer();
    virtual void ShutdownNode();
    virtual void PrepareForBeforeConnection() {}
    virtual void ReadyForGame(){}
    void SetupLoggingSystem();
    void LoadConfigurationFiles();
    virtual void LoadConfigurationData();
    virtual void OnConfigLoadSuccessful(){}
    void ConfigureEnvironment();
    void ConnectToNodeHelper(entt::registry& registry, uint32_t nodeType);
	void ConnectToNodeHelper(entt::registry& registry, const NodeInfo& nodeInfo);

    void InitializeGrpcClients();
    void ReleaseNodeId();
    void SetUpEventHandlers();
	void StopWatchingServiceNodes();
	void RegisterSelfInService();
	void ConnectToServiceNode(const std::string& json_value, uint32_t serviceNodeType);
    static void AsyncOutput(const char* msg, int len);
    void InitGrpcClients();
    void RegisterServiceNodes();
	void StartWatchingServiceNodes();
    void InitGrpcetcdserverpbResponseHandler();
    void InitializeGrpcMessageQueues();

    muduo::net::EventLoop* loop_;
    muduo::AsyncLogging muduoLog;
    RpcServerPtr rpcServer;
    TimerTaskComp deployQueueTimer;
    TimerTaskComp renewNodeLeaseTimer;
    TimerTaskComp etcdQueueTimer;
    RpcClientPtr zoneCentreNode;
};

muduo::AsyncLogging& logger(); 
