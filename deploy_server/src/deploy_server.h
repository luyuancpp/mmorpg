#ifndef DEPLOY_SERVER_SRCDEPLOY_SERVER_H_
#define DEPLOY_SERVER_SRCDEPLOY_SERVER_H_

#include "muduo/net/InetAddress.h"

#include "src/mysql_database/mysql_database.h"
#include "src/redis_client/redis_client.h"
#include "src/server_common/rpc_server.h"
#include "src/server_type_id/server_type_id.h"

namespace deploy_server
{
    static const uint32_t kRedisPort = 6379;
    static const uint32_t kGroup = 500;
    static const uint32_t kBeginPort = 2000;
    static const uint32_t kGroupServerSize = common::SERVER_ID_GROUP_SIZE - common::SERVER_REDIS + 1;
    static const uint32_t kTotalSize = kGroup * kGroupServerSize;
    static const uint32_t kLogicBeginId = kTotalSize + 1;
    static const uint32_t kLogicBeginPort = kBeginPort + kTotalSize + 1;
    static const std::string kIp = "127.0.0.1";

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
