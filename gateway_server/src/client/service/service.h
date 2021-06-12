#ifndef GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_
#define GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_

#include "c2gw.pb.h"

#include "src/codec/codec.h"

#include "muduo/base/noncopyable.h"
#include "muduo/net/EventLoop.h"

#include "src/codec/dispatcher.h"
#include "src/server_rpc_client/rpc_client_closure.h"
#include "src/server_rpc_client/stub_rpc_client.h"
#include "src/login/login_rpcclient.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
using LoginRequestPtr = std::shared_ptr<LoginRequest> ;
using CreatePlayerRequestPtr = std::shared_ptr<CreatePlayerRequest>;
using EnterGameRequestPtr = std::shared_ptr<EnterGameRequest>;

class ClientReceiver : muduo::noncopyable
{
public:
    using LoginStub = common::RpcClient<gw2l::LoginService_Stub>;
    using RpcClientPtr = std::shared_ptr<LoginStub>;

    ClientReceiver(ProtobufCodec& codec, ProtobufDispatcher& dispatcher);

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

    using CreatePlayerCC = common::ClientClosure<CreatePlayerRespone, gw2l::CreatePlayerRequest, gw2l::CreatePlayerRespone>;
    using CreatePlayerCCPtr = std::shared_ptr<CreatePlayerCC>;
    void OnServerCreatePlayerReplied(CreatePlayerCCPtr cp);

    void OnEnterGame(const muduo::net::TcpConnectionPtr& conn,
        const EnterGameRequestPtr& message,
        muduo::Timestamp);

    using EnterGameCC = common::ClientClosure<EnterGameRespone, gw2l::CreatePlayerRequest, gw2l::CreatePlayerRespone>;
    using EnterGameCCPtr = std::shared_ptr<EnterGameCC>;
    void OnServerEnterGameReplied(EnterGameCCPtr cp);

private:
    ProtobufCodec& codec_;
    ProtobufDispatcher& dispatcher_;
};
}//namespace gateway

#endif // GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_