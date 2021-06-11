#include "service.h"

#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

#include "src/server_rpc_client/rpc_client_closure.h"

#include "gw2l.pb.h"

namespace gateway
{

void ClientReceiver::OnLogin(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LoginCCPPtr p(std::make_shared<LoginCCP>(conn));
    p->server_request_.set_account(message->account());
    p->server_request_.set_password(message->password());
    LoginClient::s().Send(&ClientReceiver::LoginReplied, p, this, &gw2l::LoginService_Stub::Login);
}

void ClientReceiver::LoginReplied(LoginCCPPtr cp)
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

}


