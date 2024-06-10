#pragma once

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"

#include "src/network/centre_node.h"
#include "src/handler/game_service_handler.h"
#include "src/network/rpc_server.h"

#include "src/redis_client/redis_client.h"
#include "src/network/rpc_msg_route.h"

#include "common_proto/deploy_service.pb.h"
#include "common_proto/centre_service.pb.h"

class GameNode : muduo::noncopyable
{
public:
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

    GameNode(muduo::net::EventLoop* loop);
    ~GameNode();

    inline RpcClientPtr& lobby_node() { return lobby_node_; }
    const ::game_server_db& gs_info() const { return gs_info_; }
    inline const NodeInfo& node_info()const { return node_info_; }

    void Init();

    void InitConfig();


    void InitNetwork();

    void ServerInfo(const ::servers_info_data& info);

    void CallCentreStartGs(CentreSessionPtr controller_session);
    void CallLobbyStartGs();

    void Receive1(const OnConnected2ServerEvent& es);
    void Receive2(const OnBeConnectedEvent& es);

private:    
    void Connect2Lobby();

    muduo::net::EventLoop* loop_{ nullptr };

    PbSyncRedisClientPtr redis_;

    RpcServerPtr server_;

    RpcClientPtr deploy_node_;

    RpcClientPtr lobby_node_;

    ::game_server_db gs_info_;
    NodeInfo node_info_;

    GameServiceHandler gs_service_impl_;
};

NodeId node_id();

extern GameNode* g_game_node;
