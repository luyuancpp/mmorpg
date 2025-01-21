﻿#pragma once

#include <memory>
#include <muduo/base/AsyncLogging.h>
#include <muduo/net/TcpServer.h>
#include "common/common.pb.h"
#include "grpc/generator/deploy_service_grpc.h"
#include "network/rpc_client.h"
#include "network/rpc_server.h"
#include "time/comp/timer_task_comp.h"
#include "type_define/type_define.h"

class Node : muduo::noncopyable{
public:
    using RpcServerPtr = std::unique_ptr<muduo::net::RpcServer>;

    explicit Node(muduo::net::EventLoop* loop, const std::string& logFilePath);
    virtual ~Node();

    virtual void Init();
    virtual void StartRpcServer(const nodes_info_data& data) = 0;
    virtual void ShutdownNode();
    virtual void SetNodeId(NodeId node_id)final{GetNodeInfo().set_node_id(node_id);}
    virtual NodeId GetNodeId() final {return GetNodeInfo().node_id();}
    virtual uint32_t GetNodeType() const = 0;
    virtual NodeInfo& GetNodeInfo() = 0;
    virtual void InitializeSystemBeforeConnection() {}

protected:
    void InitLog();
    void InitializeNodeConfig();
    virtual void InitializeGameConfig();
    virtual void OnConfigLoadSuccessful();
    void InitTimeZone();
    void InitializeNodeFromRequestInfo();
    virtual void ConnectToCentralNode(::google::protobuf::Service* service);
    void InitializeGrpcNode();

    void ReleaseNodeId();

    static void AsyncOutput(const char* msg, int len);

    muduo::net::EventLoop* loop_;
    muduo::AsyncLogging muduoLog;
    RpcServerPtr rpcServer;
    NodeInfo node_info_;
    TimerTaskComp deployRpcTimer;
    TimerTaskComp renewNodeLeaseTimer;
    nodes_info_data serversInfo;
    nodes_info_data node_net_info_;
};

muduo::AsyncLogging& logger(); 
