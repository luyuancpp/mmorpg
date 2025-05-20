﻿#pragma once

#include <memory>
#include <muduo/base/AsyncLogging.h>
#include <muduo/net/TcpServer.h>
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

    NodeId GetNodeId() const { return GetNodeInfo().node_id(); }
    uint32_t GetNodeType() const { return GetNodeInfo().node_type(); }
    NodeInfo& GetNodeInfo()const;
	virtual std::string GetServiceName(uint32_t nodeType) const;
	virtual ::google::protobuf::Service* GetNodeReplyService() { return {}; }
    inline [[nodiscard]] muduo::AsyncLogging& Log() { return logSystem; }
    [[nodiscard]] RpcClient& GetZoneCentreNode() { return *zoneCentreNode; }
    std::string FormatIpAndPort() ;
	std::string GetIp();
	uint32_t GetPort();

    void HandleServiceNodeStart(const std::string& key, const std::string& value);
	void HandleServiceNodeStop(const std::string& key, const std::string& value);
	void HandleNodeRegistration(const RegisterNodeSessionRequest& request, RegisterNodeSessionResponse& response) const;
    void HandleNodeRegistrationResponse(const RegisterNodeSessionResponse& response) const;
protected:
    void InitGlobalData();
    virtual void Initialize();
    void InitRpcServer ();
    virtual void StartRpcServer();
    void Shutdown();
    virtual void PrepareForBeforeConnection() {}
    virtual void ReadyForGame(){}
    void InitLogSystem();
    void RegisterEventHandlers();
    void LoadConfigs();
    void LoadAllConfigData();
    virtual void OnConfigLoadSuccessful(){}
    void SetupTimeZone();
	void ConnectToNode(const NodeInfo& nodeInfo);
	void ConnectToGrpcNode(const NodeInfo& nodeInfo);
	void ConnectToTcpNode(const NodeInfo& nodeInfo);
	void ConnectToHttpNode(const NodeInfo& nodeInfo);
    void InitGrpcClients();
    void ReleaseNodeId();
    void RegisterGrpcHandlers();
	void StopWatchingServiceNodes();
    std::string MakeEtcdKey(const NodeInfo& nodeInfo);
	void RegisterSelf();
	void AddServiceNode(const std::string& nodeJson, uint32_t nodeType);
    static void AsyncOutput(const char* msg, int len);
    void FetchServiceNodes();
	void StartWatchingServiceNodes();
    void InitGrpcResponseHandlers();
    void InitGrpcQueues();
    void TryRegisterNodeSession(uint32_t nodeType,  const muduo::net::TcpConnectionPtr& conn) const;
    void AcquireNode();
    static void RequestEtcdLease();
	void KeepNodeAlive();   
    void RegisterNodeSessions(const muduo::net::TcpConnectionPtr& conn);
    void OnServerConnected(const OnConnected2TcpServerEvent& es);
    void OnClientConnected(const OnTcpClientConnectedEvent& es);

    virtual void ProcessGrpcNode(const NodeInfo& nodeInfo) {}
    virtual void ProcessNodeStop(uint32_t nodeType, uint32_t nodeId) {}

    muduo::net::EventLoop* eventLoop;
    muduo::AsyncLogging logSystem;
    RpcServerPtr rpcServer;
    TimerTaskComp renewLeaseTimer;
    TimerTaskComp etcdQueueTimer;
    RpcClient* zoneCentreNode{nullptr};
	CanConnectNodeTypeList targetNodeTypeWhitelist;
};

muduo::AsyncLogging& logger(); 
