#ifndef GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_
#define GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_

#include "src/network/codec/codec.h"

#include "muduo/base/noncopyable.h"
#include "muduo/net/EventLoop.h"

#include "src/network/codec/dispatcher.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_stub.h"
#include "src/network/rpc_client.h"
#include "src/snow_flake/snow_flake.h"

#include "c2gw.pb.h"
#include "login_node.pb.h"
#include "ms_node.pb.h"
#include "gs_node.pb.h"

using namespace muduo;
using namespace muduo::net;
using namespace c2gw;


namespace gateway
{
using LoginRequestPtr = std::shared_ptr<LoginRequest> ;
using CreatePlayerRequestPtr = std::shared_ptr<CreatePlayerRequest>;
using EnterGameRequestPtr = std::shared_ptr<EnterGameRequest>;
using LeaveGameRequestPtr = std::shared_ptr<LeaveGameRequest>;
using RpcClientMessagePtr = std::shared_ptr<ClientRequest>;

class ClientReceiver : muduo::noncopyable
{
public:
    using RpcStubgw2l = common::RpcStub<gw2l::LoginService_Stub>;

    ClientReceiver(ProtobufCodec& codec, ProtobufDispatcher& dispatcher, RpcStubgw2l& gw2l_login_stub);

    void OnConnection(const muduo::net::TcpConnectionPtr& conn);

    void Send2Client(muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& messag) { codec_.send(conn, messag); }

    //client to gateway 
    void OnLogin(const muduo::net::TcpConnectionPtr& conn,
        const LoginRequestPtr& message,
        muduo::Timestamp);

    using LoginRpcReplied = std::shared_ptr<common::ClosureReplied<LoginResponse, gw2l::LoginRequest, gw2l::LoginResponse>>;
    void OnServerLoginReplied(LoginRpcReplied cp);

    void OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn, 
        const CreatePlayerRequestPtr& message, 
        muduo::Timestamp);

    using CreatePlayeReplied = std::shared_ptr<common::ClosureReplied<CreatePlayerResponse, gw2l::CreatePlayerRequest, gw2l::CreatePlayerResponse>>;
    void OnServerCreatePlayerReplied(CreatePlayeReplied cp);

    void OnEnterGame(const muduo::net::TcpConnectionPtr& conn,
        const EnterGameRequestPtr& message,
        muduo::Timestamp);

    using EnterGameRpcRplied = std::shared_ptr<common::ClosureReplied<EnterGameResponse, gw2l::EnterGameRequest, gw2l::EnterGameResponse>>;
    void OnServerEnterGameReplied(EnterGameRpcRplied cp);

    void OnLeaveGame(const muduo::net::TcpConnectionPtr& conn,
        const LeaveGameRequestPtr& message,
        muduo::Timestamp);

	void OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
		const RpcClientMessagePtr& message,
		muduo::Timestamp);

    
    struct ClientGsRpcClosure
    {
        ClientGsRpcClosure(const muduo::net::TcpConnectionPtr& cc)
			: client_connection_(cc){}
        google::protobuf::Message* c_rp_{nullptr};
        const muduo::net::TcpConnectionPtr client_connection_;
    };
	using ClientGSMessageReplied = std::shared_ptr<ClientGsRpcClosure>;
	void OnRpcClientReplied(ClientGSMessageReplied cp);

	using GsPlayerServiceRpcRplied = std::shared_ptr<common::ClosureReplied<ClientResponse, gsservice::RpcClientRequest, gsservice::RpcClientResponse>>;
	void OnGsPlayerServiceReplied(GsPlayerServiceRpcRplied cp);
private:
    ProtobufCodec& codec_;
    ProtobufDispatcher& dispatcher_;
    RpcStubgw2l& gw2l_login_stub_;
    common::ServerSequence server_sequence_;
};
}//namespace gateway

#endif // GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_