#pragma once

#include <memory>
#include <muduo/base/AsyncLogging.h>
#include <muduo/net/TcpServer.h>
#include "grpc/generator/deploy_service_grpc.h"
#include "network/rpc_client.h"
#include "network/rpc_server.h"
#include "proto/common/common.pb.h"
#include "time/comp/timer_task_comp.h"
#include "type_define/type_define.h"

class Node : muduo::noncopyable{
public:
    using RpcServerPtr = std::unique_ptr<muduo::net::RpcServer>;

    explicit Node(muduo::net::EventLoop* loop, const std::string& logFilePath);
    virtual ~Node();

    virtual NodeId GetNodeId() final { return GetNodeInfo().node_id(); }
    virtual uint32_t GetNodeType() const = 0;
    virtual NodeInfo& GetNodeInfo() = 0;
    inline [[nodiscard]] muduo::AsyncLogging& Log() { return muduoLog; }
	TimerTaskComp& GetDeployRpcTimer() { return deployRpcTimer; }
	TimerTaskComp& GetRenewNodeLeaseTimer() { return renewNodeLeaseTimer; }
	[[nodiscard]] RpcClientPtr& GetZoneCentreNode() { return zoneCentreNode; }
	[[nodiscard]] const nodes_info_data& GetNodesInfo() const { return nodesInfo; }

    void InitializeDeployService(const std::string& service_address);
protected:
    virtual void Initialize();
    virtual void StartRpcServer(const nodes_info_data& data);
    virtual void ShutdownNode();
    virtual void SetNodeId(NodeId node_id)final{GetNodeInfo().set_node_id(node_id);}
    virtual void PrepareForBeforeConnection() {}
    virtual void ReadyForGame(){}
    void SetupLogging();
    virtual void LoadConfigurations();
    virtual void OnConfigLoadSuccessful(){}
    void InitializeTimeZone();
    void InitializeNodeFromRequestInfo();
    virtual void ConnectToCentreHelper(::google::protobuf::Service* service);
    void InitializeGrpcServices();
    void InitializeLaunchTime();
    void ReleaseNodeId();
    void SetupMessageHandlers();
    void SendEtcdRangeRequest(const std::string& prefix);

    static void AsyncOutput(const char* msg, int len);

    muduo::net::EventLoop* loop_;
    muduo::AsyncLogging muduoLog;
    RpcServerPtr rpcServer;
    NodeInfo node_info_;
    TimerTaskComp deployRpcTimer;
    TimerTaskComp renewNodeLeaseTimer;
    nodes_info_data nodesInfo;
    RpcClientPtr zoneCentreNode;
};

muduo::AsyncLogging& logger(); 
