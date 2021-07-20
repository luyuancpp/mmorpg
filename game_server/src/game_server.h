#ifndef GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_
#define GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"
#include "src/master/service_ms2g.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/rpc_server.h"
#include "src/server_common/rpc_stub.h"
#include "src/redis_client/redis_client.h"
#include "src/server_common/rpc_closure.h"

#include "g2ms.pb.h"


namespace game
{
class GameServer : muduo::noncopyable, public common::Receiver<GameServer>
{
public:
    using RedisClientPtr = common::RedisClientPtr;
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    using G2MSStubg2ms = common::RpcStub<g2ms::G2MSService_Stub>;

    GameServer(muduo::net::EventLoop* loop);

    void LoadConfig();

    void ConnectDeploy();

    void receive(const common::ClientConnectionES& es);

    using ServerInfoRpcClosure = common::RpcClosure<deploy::ServerInfoRequest,
        deploy::ServerInfoResponse>;
    using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
    void ServerInfo(ServerInfoRpcRC cp);

    using StartLogicServerInfoRpcClosure = common::RpcClosure<deploy::StartLogicServerRequest,
        deploy::StartLogicServerResponse>;
    using StartLogicServerRpcRC = std::shared_ptr<StartLogicServerInfoRpcClosure>;
    void StartLogicServer(StartLogicServerRpcRC cp);

private:
    muduo::net::EventLoop* loop_{ nullptr };

    RedisClientPtr redis_;

    RpcServerPtr server_;

    common::RpcClientPtr deploy_rpc_client_;
    deploy::DeployRpcStub deploy_stub_;

    common::RpcClientPtr master_rpc_client_;
    G2MSStubg2ms g2ms_stub_;

    ::serverinfo_database server_info;

    ms2g::Ms2gServiceImpl ms2g_service_impl_;
};

};//namespace game

#endif // !GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_
