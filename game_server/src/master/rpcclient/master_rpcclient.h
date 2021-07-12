#ifndef GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#include "g2ms.pb.h"

#include "src/rpc_closure_param/rpc_stub.h"
#include "src/rpc_closure_param/rpc_stub_client.h"

using namespace muduo;
using namespace muduo::net;

namespace game
{
    class MasterRpcClient : noncopyable
    {
    public:
        using RpcClientPtr = std::unique_ptr<common::RpcClient>;

        static RpcClientPtr& GetSingleton()
        {
            static RpcClientPtr singleton;
            return singleton;
        }

        static void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            GetSingleton() = std::make_unique<common::RpcClient>(loop, login_server_addr);
            GetSingleton()->connect();
        }
    };

    class LoginRpcStub : noncopyable
    {
    public:
        using RpcStub = common::RpcStub<g2ms::LoginService_Stub>;
        static RpcStub& GetSingleton()
        {
            static RpcStub singleton(*MasterRpcClient::GetSingleton());
            return singleton;
        }
    };
}// namespace game

#endif // GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

