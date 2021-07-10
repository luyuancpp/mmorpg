#ifndef GATEWAY_SRC_GAME_RPCCLIENT_GAMER_SERVER_RPCCLIENT_H_
#define GATEWAY_SRC_GAME_RPCCLIENT_GAMER_SERVER_RPCCLIENT_H_

#include "gw2g.pb.h"

#include "src/rpc_closure_param/login_client.h"

namespace gateway
{
    class GameRpcClient
    {
    public:
        using RpcStub = common::RpcClient <gw2g::LoginService_Stub>;
        using RpcClientPtr = std::shared_ptr<RpcStub>;

        void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            game_client_ = std::make_shared<RpcStub>(loop, login_server_addr);
            game_client_->connect();
        }

    private:
        RpcClientPtr game_client_;
    };
}

#endif // GATEWAY_SRC_GAME_RPCCLIENT_GAMER_SERVER_RPCCLIENT_H_
