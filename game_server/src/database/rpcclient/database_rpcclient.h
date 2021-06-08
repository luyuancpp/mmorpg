#ifndef GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#include "g2db.pb.h"

#include "src/server_rpc_client/login_client.h"

using namespace muduo;
using namespace muduo::net;

namespace game
{
    class DatabaseRpcClient
    {
    public:
        using RpcStub = common::RpcClient <g2db::LoginService_Stub>;
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

}// namespace game

#define  db_server game::DatabaseRpcClient::GetSingleton() 

#endif // GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

