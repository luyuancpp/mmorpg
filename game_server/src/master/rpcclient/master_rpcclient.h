#ifndef GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#ifdef __linux__
#include <unistd.h>
#endif//__linux__

#include "g2ms.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "src/server_rpc_client/login_client.h"

using namespace muduo;
using namespace muduo::net;

namespace game
{
    class MasterRpcClient
    {
    public:
        using RpcStub = common::RpcClient <g2ms::LoginService_Stub>;
        using RpcClientPtr = std::shared_ptr<RpcStub>;

        void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            database_client_ = std::make_shared<RpcStub>(loop, login_server_addr);
            database_client_->connect();
        }

        static MasterRpcClient& GetSingleton()
        {
            static MasterRpcClient singleton;
            return singleton;
        }


    private:
        RpcClientPtr database_client_;
    };

}// namespace game

#define  master game::MasterRpcClient::GetSingleton() 

#endif // GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

