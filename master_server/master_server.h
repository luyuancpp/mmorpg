#ifndef MASTER_SERVER_MASTER_SERVER_H_
#define MASTER_SERVER_MASTER_SERVER_H_

#include "src/game_rpc/game_rpc_server.h"

#include "src/redis_client/redis_client.h"

namespace master
{
    class MasterServer
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        MasterServer(muduo::net::EventLoop* loop,
            const muduo::net::InetAddress& listen_addr)
            : server_(loop, listen_addr),
            redis_(std::make_shared<common::RedisClient>())
        {
            redis_->Connect(listen_addr.toIp(), 1, 1);
        }

        RedisClientPtr& redis_client() { return redis_; }

        void Start();

        void RegisterService(::google::protobuf::Service*);
    private:
        common::GameRpcServer server_;
        RedisClientPtr redis_;
    };
}//namespace master

#endif//MASTER_SERVER_MASTER_SERVER_H_
