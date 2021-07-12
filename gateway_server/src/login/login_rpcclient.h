#ifndef GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
#define GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_

#include "gw2l.pb.h"

#include "src/rpc_closure_param/rpc_stub1.h"
#include "src/rpc_closure_param/rpc_stub_client1.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
    class LoginRpcClient : noncopyable
    {
    public:
        using RpcClientPtr = std::unique_ptr<common::RpcClient1>;
        
        static RpcClientPtr& GetSingleton()
        {
            static RpcClientPtr singleton;
            return singleton;
        }

        static void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            GetSingleton() = std::make_unique<common::RpcClient1>(loop, login_server_addr);
            GetSingleton()->connect();
        }
    };

    class LoginRpcStub : noncopyable
    {
    public:
        using RpcStub = common::RpcStub1<gw2l::LoginService_Stub>;
        static RpcStub& GetSingleton()
        {
            static RpcStub singleton(*LoginRpcClient::GetSingleton());
            return singleton;
        }
    };
}//namespace gateway

#endif // !GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
