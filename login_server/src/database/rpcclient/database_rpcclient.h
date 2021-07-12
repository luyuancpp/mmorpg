#ifndef LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#include "l2db.pb.h"

#include "src/rpc_closure_param/stub_rpc_client.h"

using namespace muduo;
using namespace muduo::net;

namespace login
{
    class DbRpcClient : noncopyable
    {
    public:
        using StubType = common::RpcClient<l2db::LoginService_Stub>;
        using RpcClientPtr = std::unique_ptr<StubType>;

        static RpcClientPtr& GetSingleton()
        {
            static RpcClientPtr singleton;
            return singleton;
        }

        static void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            GetSingleton() = std::make_unique<DbRpcClient::StubType>(loop, login_server_addr);
            GetSingleton()->connect();
        }

    };

}// namespace login

#endif // LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

