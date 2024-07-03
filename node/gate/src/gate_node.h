#pragma once

#include <stdio.h>

#include "muduo/base/AsyncLogging.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "handler/c2gate.h"
#include "handler/gate_service_handler.h"
#include "network/rpc_client.h"
#include "network/rpc_connection_event.h"
#include "network/codec/codec.h"
#include "network/codec/dispatcher.h"
#include "timer_task/timer_task.h"
#include "type_define/type_define.h"

#include "common_proto/deploy_service.pb.h"
#include "constants/file.h"

class GateNode : noncopyable
{
public:
    using TcpServerPtr = std::unique_ptr<TcpServer>;

    explicit GateNode(EventLoop* loop);
    ~GateNode();

    inline ProtobufCodec& Codec() { return codec_; }
    inline GateServiceHandler& GetServiceHandler() { return service_handler_; }
    inline uint32_t GetNodeId()const { return node_info_.node_id(); }
    inline RpcClientPtr& GetZoneCentreNode() { return zone_centre_node_; }
    inline const NodeInfo& GetNodeInfo()const { return node_info_; }
    inline [[nodiscard]] muduo::AsyncLogging& Log ( ) { return log_; }

    inline void Send2Client(const muduo::net::TcpConnectionPtr& conn,
                            const ::google::protobuf::Message& message) const { client_receiver_.Send2Client(conn, message); }

    void Init();
    void Exit();
    
    void InitNodeByReqInfo();
    
    void StartServer(const nodes_info_data& data);

    void SetNodeId(NodeId node_id);
    
    void Receive1(const OnConnected2ServerEvent& es) const;

private:
    void OnConnection(const TcpConnectionPtr& conn)
    {
        client_receiver_.OnConnection(conn);
    }

    void Connect2Centre();

    void Connect2Login() const;

    void OnUnknownMessage(const TcpConnectionPtr& conn,
                                 const MessagePtr& message,
                                 Timestamp)
    {
        LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
        conn->shutdown();
    }

    void InitLog();
    static void InitConfig();
    static void InitNodeConfig();
    static void InitGameConfig();
    
    inline NodeId GetNodeConfIndex() const { return GetNodeId() - 1; }

    muduo::net::EventLoop* loop_{ nullptr };
    muduo::AsyncLogging log_;

    private:
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    ClientReceiver client_receiver_;
    TcpServerPtr server_;
    nodes_info_data node_net_info_;
    NodeInfo node_info_;
    RpcClientPtr zone_centre_node_;
    GateServiceHandler service_handler_;
    TimerTask deploy_rpc_timer_;
};


extern GateNode* g_gate_node;



