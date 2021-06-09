#include "service.h"

#include "gw2l.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

namespace gateway
{

    void LoginReplied(gw2l::LoginResponse* response)
    {
        LOG_INFO << "login : " << response->DebugString();
    }

void ClientReceiver::OnLogin(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LoginResponse respone;
    codec_.send(conn, respone);

    gw2l::LoginRequest request;
    request.set_account(message->account());
    request.set_password(message->password());
    LoginClient::s().Send(request, LoginReplied, &gw2l::LoginService_Stub::Login);
}

}


