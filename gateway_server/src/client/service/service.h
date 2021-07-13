#ifndef GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_
#define GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_

#include "c2gw.pb.h"

#include "src/codec/codec.h"

#include "muduo/base/noncopyable.h"
#include "muduo/net/EventLoop.h"

#include "src/codec/dispatcher.h"
#include "src/rpc_closure_param/rpc_client_closure.h"
#include "src/rpc_closure_param/rpc_stub_client.h"
#include "src/login/login_rpcclient.h"

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

    ClientReceiver(ProtobufCodec& codec, ProtobufDispatcher& dispatcher);

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

    using DisconnectCC = common::ClientClosure<gw2l::EmptyResponse, gw2l::DisconnectRequest, gw2l::DisconnectResponse>;
    using DisconnectCCPtr = std::shared_ptr<DisconnectCC>;
    void OnDisconnectReplied(DisconnectCCPtr cp);

    void OnLeaveGame(const muduo::net::TcpConnectionPtr& conn,
        const LeaveGameRequestPtr& message,
        muduo::Timestamp);

private:
    ProtobufCodec& codec_;
    ProtobufDispatcher& dispatcher_;
};
}//namespace gateway

#endif // GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_