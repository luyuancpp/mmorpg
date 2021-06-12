#include "service.h"

#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

#include "src/server_rpc_client/rpc_client_closure.h"

#include "gw2l.pb.h"

namespace gateway
{

ClientReceiver::ClientReceiver(ProtobufCodec& codec, ProtobufDispatcher& dispatcher)
    : codec_(codec),
        dispatcher_(dispatcher)
{
    dispatcher_.registerMessageCallback<LoginRequest>(
        std::bind(&ClientReceiver::OnLogin, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<CreatePlayerRequest>(
        std::bind(&ClientReceiver::OnCreatePlayer, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<EnterGameRequest>(
        std::bind(&ClientReceiver::OnEnterGame, this, _1, _2, _3));
}

void ClientReceiver::OnLogin(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LoginCCPtr p(std::make_shared<LoginCC>(conn));
    p->s_rqst_.set_account(message->account());
    p->s_rqst_.set_password(message->password());
    LoginClient::s().Send(&ClientReceiver::OnServerLoginReplied, p, this, &gw2l::LoginService_Stub::Login);
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

}

void ClientReceiver::OnServerCreatePlayerReplied(CreatePlayerCCPtr cp)
{

}

void ClientReceiver::OnEnterGame(const muduo::net::TcpConnectionPtr& conn, 
                                const EnterGameRequestPtr& message, 
                                muduo::Timestamp)
{

}

}


