#ifndef GATEWAY_SRC_GAME_RPCCLIENT_GAMER_SERVER_RPCCLIENT_H_
#define GATEWAY_SRC_GAME_RPCCLIENT_GAMER_SERVER_RPCCLIENT_H_

#include "gw2g.pb.h"

#include "src/rpc_closure_param/stub_rpc_client.h"

namespace gateway
{
    class GameRpcClient
    {
    public:
        using StubType = common::RpcClient<gw2g::LoginService_Stub>;
        using RpcClientPtr = std::unique_ptr<StubType>;

        static RpcClientPtr& GetSingleton()
        {
            static RpcClientPtr singleton;
            return singleton;
        }

        static void InitSingleton(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            GetSingleton() = std::make_unique<GameRpcClient::StubType>(loop, login_server_addr);
        }

    };
}

#endif // GATEWAY_SRC_GAME_RPCCLIENT_GAMER_SERVER_RPCCLIENT_H_
