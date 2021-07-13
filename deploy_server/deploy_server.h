#ifndef DEPLOY_SERVER_SRCDEPLOY_SERVER_H_
#define DEPLOY_SERVER_SRCDEPLOY_SERVER_H_

#include "muduo/net/InetAddress.h"
#include "src/game_rpc/game_rpc_server.h"

#include "src/mysql_database/mysql_database.h"
#include "src/redis_client/redis_client.h"

namespace deploy_server
{
    class DeployServer
    {
    public:
        using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
        using RedisClientPtr = std::shared_ptr<common::RedisClient>;
        DeployServer(muduo::net::EventLoop* loop,
            const muduo::net::InetAddress& listen_addr,
            const common::ConnectionParameters& db_cp)
            :server_(loop, listen_addr),
            database_(std::make_shared<common::MysqlDatabase>()),
            redis_(std::make_shared<common::RedisClient>())
        {
            redis_->Connect(listen_addr.toIp(), 1, 1);
            database_->Connect(db_cp);
        }

        MysqlClientPtr& player_mysql_client() { return database_; }
        RedisClientPtr& redis_client() { return redis_; }

        void Start();

        void RegisterService(::google::protobuf::Service*);
    private:
        muduo::net::RpcServer server_;
        MysqlClientPtr database_;
        RedisClientPtr redis_;
    };
}//namespace deploy_server

#endif // !DEPLOY_SERVER_SRCDEPLOY_SERVER_H_
