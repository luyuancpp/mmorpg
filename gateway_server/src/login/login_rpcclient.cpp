#include "login_rpcclient.h"

namespace gateway
{
    void LoginClient::Connect(EventLoop* loop, const InetAddress& login_server_addr)
    {
        login_client_ = std::make_shared <LoginStub>(loop, login_server_addr);
        login_client_->connect();
    }

}//namespace gateway