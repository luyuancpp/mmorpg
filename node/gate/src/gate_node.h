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
    inline GateServiceHandler& gate_service_hanlder() { return gate_service_handler_; }
    inline uint32_t gate_node_id()const { return node_net_info_.gate_info().id(); }
    inline RpcClientPtr& centre_node() { return centre_node_; }
    inline const NodeInfo& node_info()const { return node_info_; }

	inline void set_servers_info_data(const servers_info_data& serverinfo_data) {node_net_info_ = serverinfo_data; node_info_.set_node_id(node_net_info_.gate_info().id());	}

    inline void Send2Client(muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& messag) { client_receiver_.Send2Client(conn, messag); }

    void LoadNodeConfig();

    void Init();

    void InitNodeByReqInfo();
    
    void StartServer();
    
    void Receive1(const OnConnected2ServerEvent& es);

private:
    void OnConnection(const TcpConnectionPtr& conn)
    {
        client_receiver_.OnConnection(conn);
    }

    void Connect2Centre();

    void OnUnknownMessage(const TcpConnectionPtr& conn,
                                 const MessagePtr& message,
                                 Timestamp)
    {
        LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
        conn->shutdown();
    }

    muduo::net::EventLoop* loop_{ nullptr };

    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    ClientReceiver client_receiver_;

    TcpServerPtr server_;

    servers_info_data node_net_info_;
    NodeInfo node_info_;

    RpcClientPtr centre_node_;

    GateServiceHandler gate_service_handler_;
};

const NodeInfo& node_info();

extern GateNode* g_gate_node;



