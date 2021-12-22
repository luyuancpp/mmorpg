#ifndef GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_
#define GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"

#include "src/comp/master.hpp"
#include "src/master/service_ms2g.h"
#include "src/region/service_rg2g.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/rpc_server.h"
#include "src/server_common/rpc_stub.h"
#include "src/redis_client/redis_client.h"
#include "src/server_common/rpc_closure.h"

#include "g2ms.pb.h"
#include "g2rg.pb.h"

namespace game
{
class GameServer : muduo::noncopyable, public common::Receiver<GameServer>
{
public:
    using RedisClientPtr = common::RedisClientPtr;
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    using StubG2ms = common::RpcStub<g2ms::G2msService_Stub>;
    using StubG2rg = common::RpcStub<g2rg::G2rgService_Stub>;

    GameServer(muduo::net::EventLoop* loop);

    void Init();

    void InitNetwork();

    using ServerInfoRpcClosure = common::RpcClosure<deploy::ServerInfoRequest,
        deploy::ServerInfoResponse>;
    using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
    void ServerInfo(ServerInfoRpcRC cp);

    using StartGSInfoRpcClosure = common::RpcClosure<deploy::StartGSRequest,
        deploy::StartGSResponse>;
    using StartGSRpcRC = std::shared_ptr<StartGSInfoRpcClosure>;
    void StartGSDeployReplied(StartGSRpcRC cp);

    void Register2Master(MasterClientPtr& master_rpc_client);

    void receive(const common::RpcClientConnectionES& es);

private:    
    void InitGlobalEntities();
    void InitRoomMasters(const deploy::ServerInfoResponse* resp);
    void ConnectMaster();
    void ConnectRegion();

    muduo::net::EventLoop* loop_{ nullptr };

    RedisClientPtr redis_;

    RpcServerPtr server_;

    common::RpcClientPtr deploy_rpc_client_;
    deploy::DeployRpcStub deploy_stub_;

    StubG2ms g2ms_stub_;

    common::RpcClientPtr region_rpc_client_;
    StubG2rg g2rg_stub_;

    ::game_server_db server_info_;

    ms2g::Ms2gServiceImpl ms2g_service_impl_;
    rg2g::Rg2gServiceImpl rg2g_service_impl_;
};

};//namespace game

extern game::GameServer* g_gs;

#endif // !GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_
