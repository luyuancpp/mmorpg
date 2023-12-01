#pragma once

#include "muduo/net/EventLoop.h"

#include "src/mysql_wrapper/mysql_database.h"
#include "src/redis_client/redis_client.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_msg_route.h"
#include "src/network/rpc_server.h"
#include "src/handler/database_service_handler.h"
#include "src/util/defer.h"

class DatabaseServer : muduo::noncopyable
{
public:
    using MysqlClientPtr = std::shared_ptr<MysqlDatabase>;
    using PbSyncRedisClientPtr = std::shared_ptr<MessageSyncRedisClient>;
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

    DatabaseServer(muduo::net::EventLoop* loop);
    ~DatabaseServer();

    inline MysqlClientPtr& player_mysql_client(){ return database_; }
    inline PbSyncRedisClientPtr& redis_client() { return redis_; }
    inline const NodeInfo& node_info()const { return node_info_; }
    void Init();

    void ConnectDeploy();

    void Start();

    void StartServer(const ::servers_info_data& info);

    void Receive(const OnConnected2ServerEvent& es) const;

private:
    muduo::net::EventLoop* loop_{ nullptr };
   
    MysqlClientPtr database_;
    PbSyncRedisClientPtr redis_;
    RpcServerPtr server_;

    NodeInfo node_info_;

    RpcClientPtr deploy_session_;

    DbServiceHandler impl_;
};

extern DatabaseServer* g_database_node;