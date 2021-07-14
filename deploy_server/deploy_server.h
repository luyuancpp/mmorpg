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
            const muduo::net::InetAddress& listen_addr);

        MysqlClientPtr& player_mysql_client() { return database_; }

        void Start();
        
        void RegisterService(::google::protobuf::Service*);
    private:
        void InitServerInof();

        muduo::net::RpcServer server_;
        MysqlClientPtr database_;
    };
}//namespace deploy_server

#endif // !DEPLOY_SERVER_SRCDEPLOY_SERVER_H_
