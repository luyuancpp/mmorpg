#pragma once

#include <stdio.h>

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "src/event/event.h"
#include "src/service/common_proto/c2gate.h"
#include "src/network/codec/codec.h"
#include "src/network/codec/dispatcher.h"
#include "src/service/common_proto/gate_service.h"
#include "src/network/rpc_msg_route.h"
#include "src/network/rpc_connection_event.h"

#include "deploy_service.pb.h"
#include "controller_service.pb.h"

using namespace muduo;
using namespace muduo::net;


class GateServer : noncopyable, public Receiver<GateServer>
{
public:
    using TcpServerPtr = std::unique_ptr<TcpServer>;

    GateServer(EventLoop* loop)
        : loop_(loop),
        dispatcher_(std::bind(&GateServer::OnUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        client_receiver_(codec_, dispatcher_)
    { }

    inline EventLoop* loop() { return loop_; }
    inline ProtobufCodec& codec() { return codec_; };
    inline GateServiceImpl& node_service_impl() { return gate_service_; }
    inline uint32_t gate_node_id()const { return conf_info_.gate_info().id(); }
    inline RpcClientPtr& deploy_session() { return deploy_session_; }
    inline RpcClientPtr& controller_node_session() { return controller_node_; }
    inline const NodeInfo& node_info()const { return node_info_; }

	inline void set_servers_info_data(const servers_info_data& serverinfo_data) {conf_info_ = serverinfo_data; node_info_.set_node_id(conf_info_.gate_info().id());	}

    inline void Send2Client(muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& messag) { client_receiver_.Send2Client(conn, messag); }

    void LoadConfig();

    void Init();
    
    void StartServer();



    void receive(const OnConnected2ServerEvent& es);

private:
    void OnConnection(const TcpConnectionPtr& conn)
    {
        client_receiver_.OnConnection(conn);
    }

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

    servers_info_data conf_info_;
    NodeInfo node_info_;

    RpcClientPtr deploy_session_;
    RpcClientPtr controller_node_;

    GateServiceImpl gate_service_;
};

extern GateServer* g_gate_node;



