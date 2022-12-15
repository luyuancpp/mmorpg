#pragma once

#include "muduo/net/EventLoop.h"

#include "src/event/event.h"
#include "src/mysql_wrapper/mysql_database.h"
#include "src/redis_client/redis_client.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_msg_route.h"
#include "src/network/rpc_server.h"
#include "src/service/common_proto/database_service.h"

#include "deploy_service.pb.h"

class DatabaseServer : muduo::noncopyable, public Receiver<DatabaseServer>
{
public:
    using MysqlClientPtr = std::shared_ptr<MysqlDatabase>;
    using PbSyncRedisClientPtr = std::shared_ptr<MessageSyncRedisClient>;
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

    DatabaseServer(muduo::net::EventLoop* loop);

    inline MysqlClientPtr& player_mysql_client(){ return database_; }
    inline PbSyncRedisClientPtr& redis_client() { return redis_; }

    void Init();

    void ConnectDeploy();

    void Start();

    void StartServer(const ::servers_info_data& info);

    void receive(const OnConnected2ServerEvent& es);

private:
    muduo::net::EventLoop* loop_{ nullptr };
    MysqlClientPtr database_;
    PbSyncRedisClientPtr redis_;
    RpcServerPtr server_;

    RpcClientPtr deploy_session_;

    DbServiceImpl impl_;
};

extern DatabaseServer* g_database_node;