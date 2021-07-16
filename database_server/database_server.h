#ifndef DATABASE_SERVER_DATABASE_SERVER_H_
#define DATABASE_SERVER_DATABASE_SERVER_H_

#include "muduo/net/EventLoop.h"

#include "src/event/event.h"
#include "src/mysql_database/mysql_database.h"
#include "src/redis_client/redis_client.h"
#include "src/game_rpc/game_rpc_server.h"
#include "src/net/deploy/rpcclient/deploy_rpcclient.h"
#include "src/rpc_closure_param/rpc_stub_client.h"
#include "src/rpc_closure_param/rpc_closure.h"

#include "src/login/service.h"

#include "deploy.pb.h"


namespace database
{
    class DatabaseServer : muduo::noncopyable, public common::Receiver<DatabaseServer>
    {
    public:
        using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
        using RedisClientPtr = std::shared_ptr<common::RedisClient>;
        using RprServerPtr = std::shared_ptr<muduo::net::RpcServer>;

        DatabaseServer(muduo::net::EventLoop* loop);

        MysqlClientPtr& player_mysql_client(){ return database_; }
        RedisClientPtr& redis_client() { return redis_; }

        void LoadConfig();

        void ConnectDeploy();

        void Start();

        void receive(const common::ConnectionEvent& es);

        using ServerInfoRpcClosure = common::RpcClosure<deploy::ServerInfoRequest,
            deploy::ServerInfoResponse>;
        using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
        void StartServer(ServerInfoRpcRC cp);

    private:
        muduo::net::EventLoop* loop_{ nullptr };
        MysqlClientPtr database_;
        RedisClientPtr redis_;
        RprServerPtr server_;

        common::RpcClientPtr deploy_rpc_client_;
        deploy::DeployRpcStub deploy_stub_;

        l2db::LoginServiceImpl impl_;
    };

}//namespace database

#endif//DATABASE_SERVER_DATABASE_SERVER_H_
