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
    gw2l::LoginRequest request;
    request.set_account(message->account());
    request.set_password(message->password());
    LoginClient::s().Send(request, &ClientReceiver::LoginReplied, p, this, &gw2l::LoginService_Stub::Login);
}

void ClientReceiver::LoginReplied(LoginCCPPtr cp)
{
    cp->client_respone_.set_account(cp->server_respone_->account());
    cp->client_respone_.set_password(cp->server_respone_->password());
    codec_.send(cp->client_connection_, cp->client_respone_);
}

}


