#pragma once

#include <stdio.h>

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "handler/c2gate.h"
#include "network/codec/codec.h"
#include "network/codec/dispatcher.h"
#include "handler/gate_service_handler.h"
#include "network/rpc_connection_event.h"
#include "type_define/type_define.h"
#include "timer_task/timer_task.h"

#include "common_proto/deploy_service.pb.h"

class GateNode : noncopyable
{
public:
    using TcpServerPtr = std::unique_ptr<TcpServer>;

    GateNode(EventLoop* loop)
        : loop_(loop),
        dispatcher_(std::bind(&GateNode::OnUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        client_receiver_(codec_, dispatcher_)
    { }

    ~GateNode();

    inline EventLoop* loop() { return loop_; }
    inline ProtobufCodec& codec() { return codec_; };
    inline GateServiceHandler& gate_service_hanlder() { return gate_service_; }
    inline uint32_t gate_node_id()const { return node_info_.node_id(); }
    inline RpcClientPtr& zone_centre_node() { return zone_centre_node_; }
    inline const NodeInfo& node_info()const { return node_info_; }

    inline void Send2Client(muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& messag) { client_receiver_.Send2Client(conn, messag); }

    void LoadNodeConfig();

    void Init();

    void InitNodeByReqInfo();
    
    void StartServer(const nodes_info_data& serverinfo_data);

    void SetNodeId(NodeId node_id);
    
    void Receive1(const OnConnected2ServerEvent& es);

private:
    void OnConnection(const TcpConnectionPtr& conn)
    {
        client_receiver_.OnConnection(conn);
    }

    void Connect2Centre();

    void Connect2Login();

    void OnUnknownMessage(const TcpConnectionPtr& conn,
                                 const MessagePtr& message,
                                 Timestamp)
    {
        LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
        conn->shutdown();
    }

    inline NodeId game_node_index() { return gate_node_id() - 1; }

    muduo::net::EventLoop* loop_{ nullptr };

    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    ClientReceiver client_receiver_;

    TcpServerPtr server_;

    nodes_info_data node_net_info_;
    NodeInfo node_info_;

    RpcClientPtr zone_centre_node_;

    GateServiceHandler gate_service_;

    TimerTask deploy_rpc_timer_;
};


extern GateNode* g_gate_node;



