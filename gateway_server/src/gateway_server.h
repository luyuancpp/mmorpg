#ifndef GATEWAY_GATEWAY_SERVER_H_
#define GATEWAY_GATEWAY_SERVER_H_

#include <stdio.h>

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "src/event/event.h"
#include "src/service/c2gw.h"
#include "src/network/codec/codec.h"
#include "src/network/codec/dispatcher.h"
#include "src/service/gw_service.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_connection_event.h"

#include "ms_service.pb.h"

using namespace muduo;
using namespace muduo::net;


class GatewayServer : noncopyable, public Receiver<GatewayServer>
{
public:
    using RpcStubgw2l = RpcStub<gw2l::LoginService_Stub>;
    using RpcStubMsNode = RpcStub<msservice::MasterNodeService_Stub>;
    using TcpServerPtr = std::unique_ptr<TcpServer>;

    GatewayServer(EventLoop* loop)
        : loop_(loop),
        dispatcher_(std::bind(&GatewayServer::OnUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        client_receiver_(codec_, dispatcher_)
    { }

    RpcStubMsNode& gw2ms_stub() { return gw2ms_stub_; }
    GwNodeServiceImpl& node_service_impl() { return node_service_impl_; }
    inline uint32_t gate_node_id()const { return serverinfo_data_.gateway_info().id(); }

    inline void Send2Client(muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& messag) { client_receiver_.Send2Client(conn, messag); }

    void LoadConfig();

    void Init();

    using ServerInfoRpcClosure = NormalClosure<deploy::ServerInfoRequest,
        deploy::ServerInfoResponse>;
    using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
    void StartServer(ServerInfoRpcRC cp);

    void ConnectLogin(const login_server_db& login_addr);

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

    servers_info_data serverinfo_data_;

    RpcClientPtr deploy_session_;
    DeployStub deploy_stub_;

    RpcClientPtr master_session_;
    RpcStubMsNode gw2ms_stub_;

    GwNodeServiceImpl node_service_impl_;
};

extern GatewayServer* g_gateway_server;

#endif // !GATEWAY_GATEWAY_SERVER_H_


