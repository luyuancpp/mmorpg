#ifndef GATEWAY_GATEWAY_SERVER_H_
#define GATEWAY_GATEWAY_SERVER_H_

#include <stdio.h>

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "src/event/event.h"
#include "src/service/c2gw.h"
#include "src/server_common/codec/codec.h"
#include "src/server_common/codec/dispatcher.h"
#include "src/service/ms2gw.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_connection_event.h"

#include "gw2g.pb.h"
#include "gw2ms.pb.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
class GatewayServer : noncopyable, public common::Receiver<GatewayServer>
{
public:
    using RpcStubgw2l = common::RpcStub<gw2l::LoginService_Stub>;
    using RpcStubgw2ms = common::RpcStub<gw2ms::Gw2msService_Stub>;
    using RpcStubgw2g = common::RpcStub<gw2g::Gw2gService_Stub>;
    using TcpServerPtr = std::unique_ptr<TcpServer>;

    GatewayServer(EventLoop* loop)
        : loop_(loop),
        dispatcher_(std::bind(&GatewayServer::OnUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        client_receiver_(codec_, dispatcher_, gw2l_login_stub_, gw2g_stub_)
    { }

    RpcStubgw2l& gw2l_stub() { return gw2l_login_stub_; }
    RpcStubgw2ms& gw2ms_stub() { return gw2ms_stub_; }

    template<typename ServerInfo>
    bool IsSameAddr(const InetAddress& conn_addr, const ServerInfo& server_info)
    {
        return server_info.ip() == conn_addr.toIp() && server_info.port() == conn_addr.port();
    }


    void LoadConfig();

    void InitNetwork();

    using ServerInfoRpcClosure = common::RpcClosure<deploy::ServerInfoRequest,
        deploy::ServerInfoResponse>;
    using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
    void StartServer(ServerInfoRpcRC cp);

    void Register2Master();

    void receive(const common::RpcClientConnectionEvent& es);

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

    common::RpcClientPtr deploy_session_;
    deploy::DeployStub deploy_stub_;

    common::RpcClientPtr login_session_;
    RpcStubgw2l gw2l_login_stub_;

    common::RpcClientPtr master_session_;
    ms2gw::Ms2gwServiceImpl ms2gw_service_impl_;
    RpcStubgw2ms gw2ms_stub_;

    RpcStubgw2g gw2g_stub_;
};

} // namespace gateway

extern gateway::GatewayServer* g_gateway_server;

#endif // !GATEWAY_GATEWAY_SERVER_H_


