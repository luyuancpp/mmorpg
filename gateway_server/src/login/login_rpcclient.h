#ifndef GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
#define GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_

#include "gw2l.pb.h"

#include "src/rpc_closure_param/rpc_stub_client.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
    class LoginRpcClient : noncopyable
    {
    public:
        using StubType = common::RpcClient<gw2l::LoginService_Stub>;
        using RpcClientPtr = std::unique_ptr<StubType>;

        static RpcClientPtr& GetSingleton()
        {
            static RpcClientPtr singleton;
            return singleton;
        }

        static void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            GetSingleton() = std::make_unique<LoginRpcClient::StubType>(loop, login_server_addr);
            GetSingleton()->connect();
        }
    };
}//namespace gateway

#endif // !GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
