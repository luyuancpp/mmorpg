#pragma once

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"

#include "src/network/controller_node.h"
#include "src/service/common_proto/gs_service.h"
#include "src/network/rpc_server.h"
#include "src/network/rpc_stub.h"
#include "src/redis_client/redis_client.h"
#include "src/network/rpc_closure.h"

#include "deploy_service.pb.h"
#include "controller_service.pb.h"
#include "logic_proto/lobby_scene.pb.h"

class GameServer : muduo::noncopyable, public Receiver<GameServer>
{
public:
    using PbSyncRedisClientPtr = PbSyncRedisClientPtr;
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    using ControllerStub = RpcStub<controllerservice::ControllerNodeService_Stub>;
    using LobbyNodeStub = RpcStub<lobbyservcie::LobbyService_Stub>;

    GameServer(muduo::net::EventLoop* loop);

	ControllerStub& controller_stub() { return g2controller_stub_;	}

    void Init();

    void InitConfig();

    void InitNetwork();

	using ServerInfoRpc = std::shared_ptr < NormalClosure<deploy::ServerInfoRequest,
		deploy::ServerInfoResponse> >;
    void ServerInfo(ServerInfoRpc replied);

	using StartGsRpc = std::shared_ptr<NormalClosure<deploy::StartGSRequest,
		deploy::StartGSResponse>>;
    void StartGsDeployReplied(StartGsRpc replied);

	using LobbyInfoRpc = std::shared_ptr<NormalClosure<deploy::LobbyServerRequest,
		deploy::LobbyInfoResponse>>;
	void LobbyInfoReplied(LobbyInfoRpc replied);

    void CallControllerStartGs(ControllerSessionPtr& controller_session);
    void Register2Lobby();

    void receive(const OnConnected2ServerEvent& es);
    void receive(const OnBeConnectedEvent& es);

private:    
    void Connect2Lobby();

    muduo::net::EventLoop* loop_{ nullptr };

    PbSyncRedisClientPtr redis_;

    RpcServerPtr server_;

    RpcClientPtr deploy_session_;
    RpcStub<deploy::DeployService_Stub> deploy_stub_;

    ControllerStub g2controller_stub_;

    RpcClientPtr lobby_session_;
    LobbyNodeStub lobby_stub_;

    ::game_server_db gs_info_;

    GsServiceImpl gs_service_impl_;
};

extern GameServer* g_gs;
