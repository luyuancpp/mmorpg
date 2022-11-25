#pragma once

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"

#include "src/network/controller_node.h"
#include "src/service/common_proto/game_service.h"
#include "src/network/rpc_server.h"

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

    GameServer(muduo::net::EventLoop* loop);

    inline RpcClientPtr& lobby_node() { return lobby_node_; }
    const ::game_server_db& gs_info() const { return gs_info_; }

    void Init();

    void InitConfig();

    void InitNetwork();

    void ServerInfo(const ::servers_info_data& info);

    void StartGsDeployReplied(const deploy::StartGSResponse& replied);

	void OnAcquireLobbyInfoReplied(deploy::LobbyInfoResponse& replied);

    void CallControllerStartGs(ControllerSessionPtr controller_session);
    void CallLobbyStartGs();

    void receive(const OnConnected2ServerEvent& es);
    void receive(const OnBeConnectedEvent& es);

private:    
    void Connect2Lobby();

    muduo::net::EventLoop* loop_{ nullptr };

    PbSyncRedisClientPtr redis_;

    RpcServerPtr server_;

    RpcClientPtr deploy_node_;

    RpcClientPtr lobby_node_;

    ::game_server_db gs_info_;

    GsServiceImpl gs_service_impl_;
};

extern GameServer* g_gs;
