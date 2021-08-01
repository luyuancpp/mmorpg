#ifndef GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_
#define GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_

#include "c2gw.pb.h"

#include "src/server_common/codec/codec.h"

#include "muduo/base/noncopyable.h"
#include "muduo/net/EventLoop.h"

#include "src/server_common/codec/dispatcher.h"
#include "src/server_common/rpc_client_closure.h"
#include "src/server_common/rpc_stub.h"
#include "src/server_common/rpc_client.h"

#include "gw2l.pb.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
using LoginRequestPtr = std::shared_ptr<LoginRequest> ;
using CreatePlayerRequestPtr = std::shared_ptr<CreatePlayerRequest>;
using EnterGameRequestPtr = std::shared_ptr<EnterGameRequest>;
using LeaveGameRequestPtr = std::shared_ptr<LeaveGameRequest>;

class ClientReceiver : muduo::noncopyable
{
public:
    using RpcStubgw2l = common::RpcStub<gw2l::LoginService_Stub>;

    ClientReceiver(ProtobufCodec& codec, ProtobufDispatcher& dispatcher, RpcStubgw2l& gw2l_login_stub);

    void OnConnection(const muduo::net::TcpConnectionPtr& conn);

    //client to gateway 
    void OnLogin(const muduo::net::TcpConnectionPtr& conn,
        const LoginRequestPtr& message,
        muduo::Timestamp);

    using LoginCC = common::ClientClosure<LoginResponse, gw2l::LoginRequest, gw2l::LoginResponse>;
    using LoginCCPtr = std::shared_ptr<LoginCC>;
    void OnServerLoginReplied(LoginCCPtr cp);

    void OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn,
        const CreatePlayerRequestPtr& message,
        muduo::Timestamp);

    using CreatePlayerCC = common::ClientClosure<CreatePlayerResponse, gw2l::CreatePlayerRequest, gw2l::CreatePlayerResponse>;
    using CreatePlayerCCPtr = std::shared_ptr<CreatePlayerCC>;
    void OnServerCreatePlayerReplied(CreatePlayerCCPtr cp);

    void OnEnterGame(const muduo::net::TcpConnectionPtr& conn,
        const EnterGameRequestPtr& message,
        muduo::Timestamp);

    using EnterGameCC = common::ClientClosure<EnterGameResponse, gw2l::EnterGameRequest, gw2l::EnterGameResponse>;
    using EnterGameCCPtr = std::shared_ptr<EnterGameCC>;
    void OnServerEnterGameReplied(EnterGameCCPtr cp);

    void OnLeaveGame(const muduo::net::TcpConnectionPtr& conn,
        const LeaveGameRequestPtr& message,
        muduo::Timestamp);

private:
    ProtobufCodec& codec_;
    ProtobufDispatcher& dispatcher_;
    RpcStubgw2l& gw2l_login_stub_;
    uint64_t id_{ 0 };
    
};
}//namespace gateway

#endif // GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_