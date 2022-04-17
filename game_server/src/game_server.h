#ifndef GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_
#define GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"

#include "src/network/ms_node.h"
#include "src/service/gs_node.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/rpc_server.h"
#include "src/network/rpc_stub.h"
#include "src/redis_client/redis_client.h"
#include "src/network/rpc_closure.h"

#include "ms_node.pb.h"
#include "rg_node.pb.h"


class GameServer : muduo::noncopyable, public common::Receiver<GameServer>
{
public:
    using RedisClientPtr = common::RedisClientPtr;
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    using StubMsNode = common::RpcStub<msservice::MasterNodeService_Stub>;
    using StubG2rg = common::RpcStub<rgservcie::RgService_Stub>;

    GameServer(muduo::net::EventLoop* loop);

	StubMsNode& ms_stub() { return g2ms_stub_;	}

    void Init();

    void InitNetwork();

    using ServerInfoRpcClosure = common::NormalClosure<deploy::ServerInfoRequest,
        deploy::ServerInfoResponse>;
    using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
    void ServerInfo(ServerInfoRpcRC cp);

    using StartGSInfoRpcClosure = common::NormalClosure<deploy::StartGSRequest,
        deploy::StartGSResponse>;
    using StartGSRpcRC = std::shared_ptr<StartGSInfoRpcClosure>;
    void StartGSDeployReplied(StartGSRpcRC cp);

	using RegionClosure = common::NormalClosure<deploy::RegionRequest,
		deploy::RegionInfoResponse>;
	using RegionRpcClosureRC = std::shared_ptr<RegionClosure>;
	void RegionInfoReplied(RegionRpcClosureRC cp);

    void Register2Master(MasterSessionPtr& master_rpc_client);

    void receive(const common::OnConnected2ServerEvent& es);
    void receive(const common::OnBeConnectedEvent& es);

private:    
	void InitGlobalEntities();
    void ConnectRegion();

    muduo::net::EventLoop* loop_{ nullptr };

    RedisClientPtr redis_;

    RpcServerPtr server_;

    common::RpcClientPtr deploy_session_;
    deploy::DeployStub deploy_stub_;

    StubMsNode g2ms_stub_;

    common::RpcClientPtr region_session_;
    StubG2rg g2rg_stub_;

    ::game_server_db server_deploy_;

    gsservice::GsServiceImpl gs_service_impl_;
};

extern GameServer* g_gs;

#endif // !GAME_SERVER_SRC_GAME_SERVER_GAME_SERVER_H_
