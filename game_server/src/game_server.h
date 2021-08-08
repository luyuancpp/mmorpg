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
    using StubG2ms = common::RpcStub<g2ms::G2msService_Stub>;

    GameServer(muduo::net::EventLoop* loop);

    void LoadConfig();

    void InitNetwork();

    void receive(const common::RpcClientConnectionES& es);

    using ServerInfoRpcClosure = common::RpcClosure<deploy::ServerInfoRequest,
        deploy::ServerInfoResponse>;
    using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
    void ServerInfo(ServerInfoRpcRC cp);

    using StartGameServerInfoRpcClosure = common::RpcClosure<deploy::StartGameServerRequest,
        deploy::StartGameServerResponse>;
    using StartGameServerRpcRC = std::shared_ptr<StartGameServerInfoRpcClosure>;
    void StartGameServer(StartGameServerRpcRC cp);

    void Register2Master();
private:    

    muduo::net::EventLoop* loop_{ nullptr };

    RedisClientPtr redis_;

    RpcServerPtr server_;

    common::RpcClientPtr deploy_rpc_client_;
    deploy::DeployRpcStub deploy_stub_;

    common::RpcClientPtr master_rpc_client_;
    StubG2ms g2ms_stub_;

    ::serverinfo_database server_info_;

    ms2g::Ms2gServiceImpl ms2g_service_impl_;
};

};//namespace game

extern game::GameServer* g_game_server;

#endif // !GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_
