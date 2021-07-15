#ifndef LOGIN_SERVER_LOGIN_SERVER_H
#define LOGIN_SERVER_LOGIN_SERVER_H

#include "src/event/event.h"
#include "src/game_rpc/game_rpc_server.h"
#include "src/gateway/service.h"
#include "src/rpc_closure_param/rpc_closure.h"
#include "src/rpc_closure_param/rpc_connection_event.h"
#include "src/redis_client/redis_client.h"

#include "deploy.pb.h"

namespace login
{
    class LoginServer : muduo::noncopyable, public common::Receiver<LoginServer>
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        using RprServerPtr = std::shared_ptr<muduo::net::RpcServer>;

        LoginServer(muduo::net::EventLoop* loop);
            
        RedisClientPtr& redis_client() { return redis_; }

        void Start();

        void receive(const common::ConnectionEvent& es);

        using ServerInfoRpcClosure = common::RpcClosure<deploy::ServerInfoRequest,
            deploy::ServerInfoResponse>;
        using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
        void StartServer(ServerInfoRpcRC cp);
    private:
        muduo::net::EventLoop* loop_{ nullptr };
        
        RedisClientPtr redis_;
        RprServerPtr server_;
        gw2l::LoginServiceImpl impl_;
    };
}

#endif // LOGIN_SERVER_LOGIN_SERVER_H

