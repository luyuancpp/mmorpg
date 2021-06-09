#ifndef MASTER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPCCLIENT_H_
#define MASTER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPCCLIENT_H_

#include "ms2db.pb.h"

#include "src/server_rpc_client/stub_rpc_client.h"

using namespace muduo;
using namespace muduo::net;

namespace master
{
    class DatabaseRpcClient
    {
    public:
        using RpcStub = common::RpcClient <ms2db::LoginService_Stub>;
        using RpcClientPtr = std::shared_ptr<RpcStub>;

        void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            database_client_ = std::make_shared<RpcStub>(loop, login_server_addr);
            database_client_->connect();
        }

        static DatabaseRpcClient& GetSingleton()
        {
            static DatabaseRpcClient singleton;
            return singleton;
        }

    private:
        RpcClientPtr database_client_;
    };

}// namespace master

#define  db_server master::DatabaseRpcClient::GetSingleton() 

#endif // MASTER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPCCLIENT_H_

