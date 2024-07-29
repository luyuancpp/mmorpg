#pragma once
#undef  TEXT//为了导航网格能编译过

#include "muduo/net/EventLoop.h"
#include "muduo/base/AsyncLogging.h"

#include "handler/service/game_service_handler.h"
#include "network/rpc_client.h"
#include "network/rpc_connection_event.h"
#include "network/rpc_server.h"
#include "redis_client/redis_client.h"
#include "timer_task/timer_task.h"
#include "type_define/type_define.h"

#include "proto/common/deploy_service.pb.h"

class GameNode : muduo::noncopyable
{
public:
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

    explicit
        GameNode(muduo::net::EventLoop* loop);
    ~GameNode();

    inline const NodeInfo& GetNodeInfo()const { return node_info_; }
    const game_node_db& GetNodeConf() const;
    inline NodeId GetNodeId()const { return GetNodeInfo().node_id();  }
    uint32_t GetNodeType() const{ return GetNodeInfo().game_node_type(); }

    inline [[nodiscard]] muduo::AsyncLogging& Log ( ) { return muduo_log_; }
    
    void SetNodeId(NodeId node_id);
    void StartServer(const ::nodes_info_data& info);
    
    void Init();
    void Exit();

    void Receive1(const OnConnected2ServerEvent& es);
    void Receive2(const OnBeConnectedEvent& es);

private:    
    void InitNodeByReqInfo();
    void Connect2Centre();

    void InitLog();
    static void InitNodeConfig();
    static void InitGameConfig();
    static void InitTimeZone();
    
    inline NodeId GetNodeConfIndex()const { return GetNodeId() - 1; }

    muduo::net::EventLoop* loop_{ nullptr };
    muduo::AsyncLogging muduo_log_;

private:
    PbSyncRedisClientPtr redis_;
    RpcServerPtr server_;
    nodes_info_data node_net_info_;
    NodeInfo node_info_;
    RpcClientPtr zone_centre_node_;
    GameServiceHandler game_service_;
    TimerTask deploy_rpc_timer_;
    TimerTask world_timer_;
};

extern GameNode* gGameNode;
