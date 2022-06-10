#pragma once

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"

#include "src/network/ms_node.h"
#include "src/service/gs_service.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/rpc_server.h"
#include "src/network/rpc_stub.h"
#include "src/redis_client/redis_client.h"
#include "src/network/rpc_closure.h"

#include "ms_service.pb.h"
#include "logic_proto/scene_rg.pb.h"


class GameServer : muduo::noncopyable, public Receiver<GameServer>
{
public:
    using PbSyncRedisClientPtr = PbSyncRedisClientPtr;
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    using StubMsNode = RpcStub<msservice::MasterNodeService_Stub>;
    using RgNodeStub = RpcStub<regionservcie::RgService_Stub>;

    GameServer(muduo::net::EventLoop* loop);

	StubMsNode& ms_stub() { return g2ms_stub_;	}

    void Init();

    void InitConfig();

    void InitNetwork();

    using ServerInfoRpcClosure = NormalClosure<deploy::ServerInfoRequest,
        deploy::ServerInfoResponse>;
    using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
    void ServerInfo(ServerInfoRpcRC cp);

    using StartGSInfoRpcClosure = NormalClosure<deploy::StartGSRequest,
        deploy::StartGSResponse>;
    using StartGSRpcRC = std::shared_ptr<StartGSInfoRpcClosure>;
    void StartGSDeployReplied(StartGSRpcRC cp);

	using RegionClosure = NormalClosure<deploy::RegionRequest,
		deploy::RegionInfoResponse>;
	using RegionRpcClosureRC = std::shared_ptr<RegionClosure>;
	void RegionInfoReplied(RegionRpcClosureRC cp);

    void Register2Master(MasterSessionPtr& master_rpc_client);
    void Register2Region();

    void receive(const OnConnected2ServerEvent& es);
    void receive(const OnBeConnectedEvent& es);

private:    
    void Connect2Region();

    muduo::net::EventLoop* loop_{ nullptr };

    PbSyncRedisClientPtr redis_;

    RpcServerPtr server_;

    RpcClientPtr deploy_session_;
    DeployStub deploy_stub_;

    StubMsNode g2ms_stub_;

    RpcClientPtr region_session_;
    RgNodeStub rg_stub_;

    ::game_server_db gs_info_;

    GsServiceImpl gs_service_impl_;
};

extern GameServer* g_gs;
