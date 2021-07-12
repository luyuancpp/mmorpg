#ifndef GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
#define GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_

#include "gw2l.pb.h"

#include "src/rpc_closure_param/stub_rpc_client.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
    class LoginClient
    {
    public:
        using StubType = common::RpcClient<gw2l::LoginService_Stub>;
        using RpcClientPtr = std::unique_ptr<StubType>;

        static RpcClientPtr& GetSingleton()
        {
            static RpcClientPtr singleton;
            return singleton;
        }

        static void InitSingleton(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            GetSingleton() = std::make_unique<LoginClient::StubType>(loop, login_server_addr);
        }
    };
}//namespace gateway

#endif // !GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
