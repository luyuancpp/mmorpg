#include "msg_receiver.h"

#include "gw2l.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"
#include "src/login/rpcclient/login_client.h"

namespace gateway
{
void MsgReceiver::ConnectLogin(EventLoop* loop, const InetAddress& login_server_addr)
{
    login_client_ = std::make_shared <RpcClient>(loop, login_server_addr);
    login_client_->connect();
}

void MsgReceiver::OnAnswer(const muduo::net::TcpConnectionPtr& conn,
    const LoginRequestPtr& message,
    muduo::Timestamp)
{
    LOG_INFO << "login : " << message->DebugString();
    LoginResponse respone;
    codec_.send(conn, respone);

    gw2l::LoginRequest rq;
    rq.set_account(message->account());
    rq.set_password(message->password());
    gw2l::LoginResponse *rsp = new gw2l::LoginResponse;
    login_client_->SendRequest<gw2l::LoginRequest, gw2l::LoginResponse>(rq,
        *rsp, NewCallback(this, &MsgReceiver::Replied, rsp));
}

void MsgReceiver::Replied(gw2l::LoginResponse* response)
{

}

}


