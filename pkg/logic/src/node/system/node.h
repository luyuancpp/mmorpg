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

class RegisterNodeSessionRequest;
class RegisterNodeSessionResponse;

class Node : muduo::noncopyable{
public:
    using RpcServerPtr = std::unique_ptr<muduo::net::RpcServer>;
    using ServiceList = std::vector<::google::protobuf::Service*>;
	using CanConnectNodeTypeList = std::set<uint32_t>;

    explicit Node(muduo::net::EventLoop* loop, const std::string& logFilePath);
    virtual ~Node();

    virtual NodeId GetNodeId() final { return GetNodeInfo().node_id(); }
    virtual uint32_t GetNodeType() const = 0;
    virtual NodeInfo& GetNodeInfo() = 0;
	virtual std::string GetServiceName(uint32_t nodeType) const;
	virtual ::google::protobuf::Service* GetNodeRepleyService() { return {}; }
	virtual CanConnectNodeTypeList GetAllowedTargetNodeTypes() { return {}; }
    inline [[nodiscard]] muduo::AsyncLogging& Log() { return muduoLog; }
    [[nodiscard]] RpcClient& GetZoneCentreNode() { return *zoneCentreNode; }
    std::string FormatIpAndPort() ;
	std::string GetIp();
	uint32_t GetPort();

    void HandleServiceNodeStart(const std::string& key, const std::string& value);
	void HandleServiceNodeStop(const std::string& key, const std::string& value);
	void HandleNodeRegistration(const RegisterNodeSessionRequest& request, RegisterNodeSessionResponse& response);
    void HandleNodeRegistrationResponse(const RegisterNodeSessionResponse& response);
protected:
    virtual void Initialize();
    void SetupRpcServer ();
    virtual void StartRpcServer();
    virtual void ShutdownNode();
    virtual void PrepareForBeforeConnection() {}
    virtual void ReadyForGame(){}
    void SetupLoggingSystem();
    void RegisterEventHandlers();
    void LoadConfigurationFiles();
    virtual void LoadConfigurationData();
    virtual void OnConfigLoadSuccessful(){}
    void ConfigureEnvironment();
	void ConnectToNode(const NodeInfo& nodeInfo);
    void InitializeGrpcClients();
    void ReleaseNodeId();
    void SetUpEventHandlers();
	void StopWatchingServiceNodes();
    std::string BuildServiceNodeKey(const NodeInfo& nodeInfo);
	void RegisterSelfInService();
	void AddServiceNode(const std::string& nodeJson, uint32_t nodeType);
    static void AsyncOutput(const char* msg, int len);
    void InitGrpcClients();
    void FetchesServiceNodes();
	void StartWatchingServiceNodes();
    void InitializeGrpcResponseHandlers();
    void InitializeGrpcMessageQueues();
    void AttemptNodeRegistration(
        uint32_t nodeType,
        const muduo::net::TcpConnectionPtr& conn);
    void AcquireNode();
    void RegisterNodeSessions(const muduo::net::TcpConnectionPtr& conn);
    void OnConnectedToServer(const OnConnected2TcpServerEvent& es);
    void OnClientConnected(const OnBeConnectedEvent& es);

    muduo::net::EventLoop* loop_;
    muduo::AsyncLogging muduoLog;
    RpcServerPtr rpcServer;
    TimerTaskComp renewNodeLeaseTimer;
    TimerTaskComp etcdQueueTimer;
    RpcClient* zoneCentreNode{nullptr};
	int64_t nodeVersion{ 0 };
};

muduo::AsyncLogging& logger(); 
