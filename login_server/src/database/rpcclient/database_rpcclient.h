#ifndef LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#include "l2db.pb.h"

#include "src/server_rpc_client/login_client.h"

using namespace muduo;
using namespace muduo::net;

namespace login
{
    class DbRpcClient
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

        static DbRpcClient& s()
        {
            static DbRpcClient singleton;
            return singleton;
        }

        RpcClientPtr& db_client() { return database_client_; }

        void Login(const l2db::LoginRequest& request);
        void LoginReplied(l2db::LoginResponse* response);

    private:
        RpcClientPtr database_client_;
    };

}// namespace login

#endif // LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

