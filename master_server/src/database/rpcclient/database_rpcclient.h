#ifndef MASTER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPCCLIENT_H_
#define MASTER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPCCLIENT_H_

#include "ms2db.pb.h"

#include "src/rpc_closure_param/rpc_stub.h"
#include "src/rpc_closure_param/rpc_stub_client.h"

using namespace muduo;
using namespace muduo::net;

namespace master
{
    class DbRpcClient : noncopyable
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
        using RpcStub = common::RpcStub<ms2db::LoginService_Stub>;
        static RpcStub& GetSingleton()
        {
            static RpcStub singleton(*DbRpcClient::GetSingleton());
            return singleton;
        }
    };
}// namespace master

#endif // MASTER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPCCLIENT_H_

