#ifndef MASTER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPCCLIENT_H_
#define MASTER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPCCLIENT_H_

#include "ms2db.pb.h"

#include "src/rpc_closure_param/stub_rpc_client.h"

using namespace muduo;
using namespace muduo::net;

namespace master
{
   
    class DbRpcClient
    {
    public:
        using StubType = common::RpcClient< ms2db::LoginService_Stub>;
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

}// namespace master

#endif // MASTER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPCCLIENT_H_

