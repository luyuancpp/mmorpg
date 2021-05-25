#ifndef LOGIN_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define LOGIN_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#ifdef __linux__
#include <unistd.h>
#endif//__linux__

#include "l2db.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "src/server_rpc_client/login_client.h"

using namespace muduo;
using namespace muduo::net;

namespace login
{
    class DatabaseRpcClient
    {
    public:
        using RpcStub = common::RpcClient <l2db::LoginService_Stub>;
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

        void Login(const l2db::LoginRequest& request);
        void LoginReplied(l2db::LoginResponse* response);

    private:
        RpcClientPtr database_client_;
    };

}// namespace login

#define  database login::DatabaseRpcClient::GetSingleton() 

#endif // LOGIN_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

