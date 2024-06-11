#pragma once

#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"

#include "src/handler/game_service_handler.h"
#include "src/network/rpc_server.h"
#include "src/network/rpc_client.h"

#include "src/redis_client/redis_client.h"
#include "src/network/rpc_msg_route.h"

#include "common_proto/deploy_service.pb.h"

class GameNode : muduo::noncopyable
{
public:
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

    GameNode(muduo::net::EventLoop* loop);
    ~GameNode();

    inline const NodeInfo& node_info()const { return node_info_; }

    void Init();

    void InitConfig();

    void StartServer(const ::servers_info_data& info);

    void RegisterGameToCentre(RpcClientPtr& controller_session);

    void Receive1(const OnConnected2ServerEvent& es);
    void Receive2(const OnBeConnectedEvent& es);

    const game_server_db& game_node_info() const { return   node_net_info_.game_info(); }
private:    
    void InitNodeByReqInfo();

    muduo::net::EventLoop* loop_{ nullptr };

    PbSyncRedisClientPtr redis_;

    RpcServerPtr server_;
    servers_info_data node_net_info_;

    NodeInfo node_info_;

    GameServiceHandler game_service_;
};

NodeId game_node_id();

extern GameNode* g_game_node;
