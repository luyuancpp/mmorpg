#ifndef DATABASE_SERVER_DATABASE_SERVER_H_
#define DATABASE_SERVER_DATABASE_SERVER_H_

#include "muduo/net/EventLoop.h"

#include "src/event/event.h"
#include "src/mysql_wrapper/mysql_database.h"
#include "src/redis_client/redis_client.h"
#include "src/network/rpc_server.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_closure.h"

#include "src/service/db_node.h"

#include "deploy_node.pb.h"

namespace database
{
    class DatabaseServer : muduo::noncopyable, public common::Receiver<DatabaseServer>
    {
    public:
        using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
        using RedisClientPtr = std::shared_ptr<common::RedisClient>;
        using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

        DatabaseServer(muduo::net::EventLoop* loop);

        MysqlClientPtr& player_mysql_client(){ return database_; }
        RedisClientPtr& redis_client() { return redis_; }

        void Init();

        void ConnectDeploy();

        void Start();

        using ServerInfoRpcClosure = common::NormalClosure<deploy::ServerInfoRequest,
            deploy::ServerInfoResponse>;
        using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
        void StartServer(ServerInfoRpcRC cp);

        void receive(const common::OnConnected2ServerEvent& es);

    private:
        muduo::net::EventLoop* loop_{ nullptr };
        MysqlClientPtr database_;
        RedisClientPtr redis_;
        RpcServerPtr server_;

        common::RpcClientPtr deploy_rpc_client_;
        deploy::DeployStub deploy_stub_;

        DbServiceImpl impl_;
    };

}//namespace database

#endif//DATABASE_SERVER_DATABASE_SERVER_H_
