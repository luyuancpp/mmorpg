#ifndef GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#include "g2ms.pb.h"

#include "src/rpc_closure_param/rpc_stub_client.h"

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

#endif // GAME_SERVER_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

