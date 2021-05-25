#include "msg_receiver.h"

#include "gw2l.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

namespace gateway
{
void MsgReceiver::ConnectLogin(EventLoop* loop, const InetAddress& login_server_addr)
{
    login_client_ = std::make_shared <LoginStub>(loop, login_server_addr);
    login_client_->connect();
}

void MsgReceiver::OnAnswer(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LOG_INFO << "login : " << message->DebugString();
    LoginResponse respone;
    codec_.send(conn, respone);

    gw2l::LoginRequest request;
    request.set_account(message->account());
    request.set_password(message->password());
    login_client_->SendRequest<gw2l::LoginRequest, gw2l::LoginResponse>
        (request, this, &MsgReceiver::Replied, &gw2l::LoginService_Stub::Login);
}

void MsgReceiver::Replied(gw2l::LoginResponse* response)
{

}

}


