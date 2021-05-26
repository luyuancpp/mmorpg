#include "login_rpcclient.h"

namespace gateway
{
    void LoginRpcClient::ConnectLogin(EventLoop* loop, const InetAddress& login_server_addr)
    {
        login_client_ = std::make_shared <LoginStub>(loop, login_server_addr);
        login_client_->connect();
    }

    void LoginRpcClient::Replied(gw2l::LoginResponse* response)
    {

    }

}//namespace gateway