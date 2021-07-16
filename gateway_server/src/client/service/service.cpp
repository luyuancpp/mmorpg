#include "service.h"

#include  <functional>
#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

#include "src/server_common/rpc_client_closure.h"

#include "gw2l.pb.h"

namespace gateway
{

ClientReceiver::ClientReceiver(ProtobufCodec& codec, ProtobufDispatcher& dispatcher, RpcStubgw2l& login_stub_gw2l)
    : codec_(codec),
      dispatcher_(dispatcher),
      login_stub_gw2l_(login_stub_gw2l)
{
    dispatcher_.registerMessageCallback<LoginRequest>(
        std::bind(&ClientReceiver::OnLogin, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<CreatePlayerRequest>(
        std::bind(&ClientReceiver::OnCreatePlayer, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<EnterGameRequest>(
        std::bind(&ClientReceiver::OnEnterGame, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<LeaveGameRequest>(
        std::bind(&ClientReceiver::OnLeaveGame, this, _1, _2, _3));
}

void ClientReceiver::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if (!conn->connected())
    {
        DisconnectCCPtr p(std::make_shared<DisconnectCC>(conn));
        p->s_reqst_.set_connection_id(p->connection_hash_id());
        login_stub_gw2l_.CallMethod(&ClientReceiver::OnDisconnectReplied,
            p,
            this,
            &gw2l::LoginService_Stub::Disconnect);
    }
}

void ClientReceiver::OnLogin(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LoginCCPtr p(std::make_shared<LoginCC>(conn));
    p->s_reqst_.set_account(message->account());
    p->s_reqst_.set_password(message->password());
    p->s_reqst_.set_connection_id(p->connection_hash_id());
    login_stub_gw2l_.CallMethod(&ClientReceiver::OnServerLoginReplied,
        p, 
        this, 
        &gw2l::LoginService_Stub::Login);
}

void ClientReceiver::OnServerLoginReplied(LoginCCPtr cp)
{
    cp->c_resp_.set_account(cp->s_resp_->account_player().account());
    cp->c_resp_.set_password(cp->s_resp_->account_player().password());
    auto& player_list = cp->s_resp_->account_player().simple_players().players();
    for (auto it : player_list)
    {
        auto p = cp->c_resp_.add_players();
        p->set_player_id(it.player_id());
    }
    codec_.send(cp->client_connection_, cp->c_resp_);
}

void ClientReceiver::OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn, 
                                    const CreatePlayerRequestPtr& message, 
                                    muduo::Timestamp)
{
    CreatePlayerCCPtr p(std::make_shared<CreatePlayerCC>(conn));
    p->s_reqst_.set_connection_id(p->connection_hash_id());
    login_stub_gw2l_.CallMethod(&ClientReceiver::OnServerCreatePlayerReplied,
        p, 
        this, 
        &gw2l::LoginService_Stub::CreatPlayer);
}

void ClientReceiver::OnServerCreatePlayerReplied(CreatePlayerCCPtr cp)
{
    auto& player_list = cp->s_resp_->account_player().simple_players().players();
    for (auto it : player_list)
    {
        auto p = cp->c_resp_.add_players();
        p->set_player_id(it.player_id());
    }
    codec_.send(cp->client_connection_, cp->c_resp_);
}

void ClientReceiver::OnEnterGame(const muduo::net::TcpConnectionPtr& conn, 
                                const EnterGameRequestPtr& message, 
                                muduo::Timestamp)
{
    EnterGameCCPtr p(std::make_shared<EnterGameCC>(conn));
    p->s_reqst_.set_connection_id(p->connection_hash_id());
    p->s_reqst_.set_player_id(message->player_id());
    login_stub_gw2l_.CallMethod(&ClientReceiver::OnServerEnterGameReplied,
        p,
        this,
        &gw2l::LoginService_Stub::EnterGame);
}

void ClientReceiver::OnServerEnterGameReplied(EnterGameCCPtr cp)
{
    codec_.send(cp->client_connection_, cp->c_resp_);
}

void ClientReceiver::OnDisconnectReplied(DisconnectCCPtr cp)
{

}

void ClientReceiver::OnLeaveGame(const muduo::net::TcpConnectionPtr& conn, 
    const LeaveGameRequestPtr& message, 
    muduo::Timestamp)
{
    LeaveGameResponse response;
    codec_.send(conn, response);
}

}


