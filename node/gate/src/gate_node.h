#pragma once

#include <stdio.h>

#include "muduo/base/AsyncLogging.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "handler/service/client_message_processor.h"
#include "handler/service/gate_service_handler.h"
#include "network/rpc_client.h"
#include "network/rpc_connection_event.h"
#include "network/codec/codec.h"
#include "network/codec/dispatcher.h"
#include "time/comp/timer_task_comp.h"

#include "proto/common/deploy_service.pb.h"
#include "log/constants/log_constants.h"
#include "node/system/node.h"

class GateNode : public  Node
{
public:
    using TcpServerPtr = std::unique_ptr<TcpServer>;

    explicit GateNode(EventLoop* loop);
    ~GateNode() override;
    inline ProtobufCodec& Codec() { return codec_; }
    inline GateServiceHandler& GetServiceHandler() { return service_handler_; }
    inline RpcClientPtr& GetZoneCentreNode() { return zoneCentreNode; }
    inline  NodeInfo& GetNodeInfo() { return node_info_; }
    uint32_t GetNodeType() const override;

    inline void SendMessageToClient(const muduo::net::TcpConnectionPtr& conn,
                            const ::google::protobuf::Message& message) const { rpcClientHandler.SendMessageToClient(conn, message); }
    void Initialize()override;
    void ShutdownNode()override;
    void StartRpcServer(const nodes_info_data& data)override;

    void Receive1(const OnConnected2ServerEvent& es) ;
private:
    void OnConnection(const TcpConnectionPtr& conn)
    {
        rpcClientHandler.OnConnection(conn);
    }

    void Connect2Login();

    void OnUnknownMessage(const TcpConnectionPtr& conn,
                                 const MessagePtr& message,
                                 Timestamp)
    {
        LOG_ERROR << "onUnknownMessage: " << message->GetTypeName().data();
        conn->shutdown();
    }


private:
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    RpcClientSessionHandler rpcClientHandler;
    NodeInfo node_info_;
    GateServiceHandler service_handler_;
    TimerTaskComp loginGrpcSelectTimer;
};

extern GateNode* g_gate_node;
