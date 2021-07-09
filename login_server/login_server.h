#ifndef LOGIN_SERVER_LOGIN_SERVER_H
#define LOGIN_SERVER_LOGIN_SERVER_H

#include "src/game_rpc/game_rpc_server.h"

#include "src/redis_client/redis_client.h"

namespace login
{
    class LoginServer : muduo::noncopyable
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        LoginServer(muduo::net::EventLoop* loop,
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
        common::RpcServer server_;
        RedisClientPtr redis_;
    };
}

#endif // LOGIN_SERVER_LOGIN_SERVER_H

