#ifndef GATEWAY_GATEWAY_SERVER_H_
#define GATEWAY_GATEWAY_SERVER_H_

#include <stdio.h>

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "src/event/event.h"
#include "src/service/c2gate.h"
#include "src/network/codec/codec.h"
#include "src/network/codec/dispatcher.h"
#include "src/service/gate_service.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_connection_event.h"

#include "deploy_service.pb.h"
#include "controller_service.pb.h"

using namespace muduo;
using namespace muduo::net;


class GateServer : noncopyable, public Receiver<GateServer>
{
public:
    using RpcStubgw2l = RpcStub<gw2l::LoginService_Stub>;
    using RpcStubControllerNode = RpcStub<controllerservice::ControllerNodeService_Stub>;
    using TcpServerPtr = std::unique_ptr<TcpServer>;

    GateServer(EventLoop* loop)
        : loop_(loop),
        dispatcher_(std::bind(&GateServer::OnUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        client_receiver_(codec_, dispatcher_)
    { }

    RpcStubControllerNode& controller_stub() { return controller_stub_; }
    GwNodeServiceImpl& node_service_impl() { return node_service_impl_; }
    inline uint32_t gate_node_id()const { return serverinfo_data_.gate_info().id(); }

    inline void Send2Client(muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& messag) { client_receiver_.Send2Client(conn, messag); }

    void LoadConfig();

    void Init();

	using ServerInfoRpc = std::shared_ptr<NormalClosure<deploy::ServerInfoRequest,deploy::ServerInfoResponse>>;
    void StartServer(ServerInfoRpc replied);

	using LoginNodeInfoRpc = std::shared_ptr<NormalClosure<deploy::GroupLignRequest,
		deploy::GruoupLoginNodeResponse>>;
    void LoginNoseInfoReplied(LoginNodeInfoRpc replied);

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
    RpcStub<deploy::DeployService_Stub> deploy_stub_;

    RpcClientPtr controller_node_session_;
    RpcStubControllerNode controller_stub_;

    GwNodeServiceImpl node_service_impl_;
};

extern GateServer* g_gateway_server;

#endif // !GATEWAY_GATEWAY_SERVER_H_


