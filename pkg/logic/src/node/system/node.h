#pragma once

#include <memory>
#include <muduo/base/AsyncLogging.h>
#include <muduo/net/TcpServer.h>
#include "common/common.pb.h"
#include "grpc/generator/deploy_service_grpc.h"
#include "network/rpc_client.h"
#include "time/comp/timer_task_comp.h"
#include "type_define/type_define.h"

class Node {
public:
    Node(muduo::net::EventLoop* loop);
    virtual ~Node();

    virtual void Init();
    virtual void StartServer(const nodes_info_data& data) = 0;
    virtual void Exit();
    virtual void SetNodeId(NodeId node_id);
    virtual NodeId GetNodeId() const = 0;
    virtual uint32_t GetNodeType() const = 0;
    virtual NodeInfo& GetNodeInfo() = 0;

protected:
    void InitLog();
    virtual void InitNodeConfig();
    virtual void InitGameConfig();
    virtual void InitTimeZone();
    virtual void InitNodeByReqInfo();
    virtual void Connect2Centre();
    virtual void ReleaseNodeId() const;

    static void AsyncOutput(const char* msg, int len);

    // 提取共用的成员变量
    muduo::net::EventLoop* loop_;
    muduo::AsyncLogging muduoLog;
    std::shared_ptr<RpcClientPtr::element_type> rpcClientHandler;
    std::unique_ptr<muduo::net::TcpServer> server_;
    NodeInfo node_info_;
    RpcClientPtr centreNodePtr_;
    TimerTaskComp deployRpcTimer;
    TimerTaskComp renewNodeLeaseTimer;
};

muduo::AsyncLogging& logger(); 
