#ifndef MASTER_SERVER_MASTER_SERVER_H_
#define MASTER_SERVER_MASTER_SERVER_H_

#include "src/event/event.h"
#include "src/game_rpc/game_rpc_server.h"
#include "src/login/service.h"
#include "src/rpc_closure_param/rpc_closure.h"
#include "src/rpc_closure_param/rpc_connection_event.h"
#include "src/redis_client/redis_client.h"

#include "deploy.pb.h"
#include "l2ms.pb.h"

namespace master
{
    class MasterServer : muduo::noncopyable, public common::Receiver<MasterServer>
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        using RprServerPtr = std::shared_ptr<muduo::net::RpcServer>;

        MasterServer(muduo::net::EventLoop* loop);           

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
        l2ms::LoginServiceImpl impl_;
    };
}//namespace master

#endif//MASTER_SERVER_MASTER_SERVER_H_
