#ifndef DEPLOY_SERVER_SRCDEPLOY_SERVER_H_
#define DEPLOY_SERVER_SRCDEPLOY_SERVER_H_

#include "muduo/net/InetAddress.h"

#include "src/game_logic/timer_task/timer_task.h"
#include "src/mysql_database/mysql_database.h"
#include "src/redis_client/redis_client.h"
#include "src/reuse_game_id/reuse_game_id.h"
#include "src/server_common/rpc_server.h"
#include "src/server_common/rpc_connection_event.h"

namespace deploy
{
    class DeployServer : public common::Receiver<DeployServer>
    {
    public:
        using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
        using RedisClientPtr = std::shared_ptr<common::RedisClient>;

        DeployServer(muduo::net::EventLoop* loop,
            const muduo::net::InetAddress& listen_addr);

        MysqlClientPtr& player_mysql_client() { return database_; }
        ReuseGameServerId& reuse_game_id() { return reuse_id_; }

        void Start();

        void RegisterService(::google::protobuf::Service*);

        uint32_t CreateGameServerId();

        void LoadGameServerDb();
        void SaveGameServerDb();

        void OnDisConnected(const muduo::net::TcpConnectionPtr& conn);
        void LogReuseInfo();
   
        void receive(const common::ServerConnectionES& es);
    private:
        void InitGroupServerDb();        
        

        muduo::net::RpcServer server_;
        MysqlClientPtr database_;
        std::string nomoral_database_ip_;
        std::string nomoral_password_;
        std::string redis_ip_ = "127.0.0.1";
        std::string nomoral_ip_ = "127.0.0.1";
        ReuseGameServerId reuse_id_;
        common::TimerTask scan_over_timer_;
    };
}//namespace deploy

extern deploy::DeployServer* g_deploy_server;

#endif // !DEPLOY_SERVER_SRCDEPLOY_SERVER_H_
