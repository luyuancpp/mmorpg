#include "service.h"

#include "gw2l.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

namespace gateway
{

void ClientReceiver::OnAnswer(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LOG_INFO << "login : " << message->DebugString();
    LoginResponse respone;
    codec_.send(conn, respone);

    gw2l::LoginRequest request;
    request.set_account(message->account());
    request.set_password(message->password());
    login.SendRequest<gw2l::LoginRequest, gw2l::LoginResponse>
        (request, &login, &LoginRpcClient::Replied, &gw2l::LoginService_Stub::Login);
}

}


