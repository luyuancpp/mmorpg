#pragma once

#include "muduo/net/EventLoop.h"

#include "src/event/event.h"
#include "src/mysql_wrapper/mysql_database.h"
#include "src/redis_client/redis_client.h"
#include "src/network/rpc_server.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_closure.h"

#include "src/service/db_service.h"

#include "deploy_service.pb.h"

class DatabaseServer : muduo::noncopyable, public Receiver<DatabaseServer>
{
public:
    using MysqlClientPtr = std::shared_ptr<MysqlDatabase>;
    using PbSyncRedisClientPtr = std::shared_ptr<MessageSyncRedisClient>;
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

    DatabaseServer(muduo::net::EventLoop* loop);

    MysqlClientPtr& player_mysql_client(){ return database_; }
    PbSyncRedisClientPtr& redis_client() { return redis_; }

    void Init();

    void ConnectDeploy();

    void Start();

	using ServerInfoRpc = std::shared_ptr<NormalClosure<deploy::ServerInfoRequest,
		deploy::ServerInfoResponse>>;
    void StartServer(ServerInfoRpc replied);

    void receive(const OnConnected2ServerEvent& es);

private:
    muduo::net::EventLoop* loop_{ nullptr };
    MysqlClientPtr database_;
    PbSyncRedisClientPtr redis_;
    RpcServerPtr server_;

    RpcClientPtr deploy_rpc_client_;
    RpcStub<deploy::DeployService_Stub> deploy_stub_;

    DbServiceImpl impl_;
};

