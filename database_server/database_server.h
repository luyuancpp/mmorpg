#ifndef DATABASE_SERVER_DATABASE_SERVER_H_
#define DATABASE_SERVER_DATABASE_SERVER_H_

#include "src/mysql_database/mysql_database.h"
#include "src/redis_client/redis_client.h"

#include "muduo/net/EventLoop.h"
#include "src/game_rpc/game_rpc_server.h"

namespace database
{
    class DatabaseServer : muduo::noncopyable
    {
    public:
        using MysqlClientPtr = std::shared_ptr<MysqlDatabase>;
        using RedisClientPtr = std::shared_ptr<common::RedisClient>;
        DatabaseServer(muduo::net::EventLoop* loop,
            const muduo::net::InetAddress& listen_addr,
            const common::ConnectionParameters& db_cp)
            :server_(loop, listen_addr),
            database_(std::make_shared<MysqlDatabase>()),
            redis_(std::make_shared<common::RedisClient>())
        {
            redis_->Connect(listen_addr.toIp(), 1, 1);
            database_->Connect(db_cp);
        }

        MysqlClientPtr& player_mysql_client(){ return database_; }
        RedisClientPtr& redis_client() { return redis_; }

        void Start();

        void RegisterService(::google::protobuf::Service*);
    private:
        common::RpcServer server_;
        MysqlClientPtr database_;
        RedisClientPtr redis_;
    };

}//namespace database

#endif//DATABASE_SERVER_DATABASE_SERVER_H_
