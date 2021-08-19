#ifndef DEPLOY_SERVER_SRCDEPLOY_SERVER_H_
#define DEPLOY_SERVER_SRCDEPLOY_SERVER_H_

#include "muduo/net/InetAddress.h"

#include "src/game_logic/reuse_id/reuse_id.h"
#include "src/mysql_database/mysql_database.h"
#include "src/redis_client/redis_client.h"
#include "src/server_common/rpc_server.h"
#include "src/server_common/rpc_connection_event.h"

namespace deploy
{
    class DeployServer : public common::Receiver<DeployServer>
    {
    public:
        using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
        using RedisClientPtr = std::shared_ptr<common::RedisClient>;
        using ReuseId = common::ReuseId< uint32_t, ::google::protobuf::Map<uint32_t, bool>, UINT16_MAX>;
        using GameServerEntities = std::unordered_map<std::string, uint32_t>;
        DeployServer(muduo::net::EventLoop* loop,
            const muduo::net::InetAddress& listen_addr);

        MysqlClientPtr& player_mysql_client() { return database_; }
        ReuseId& reuse_id() { return reuse_id_; }
        GameServerEntities& game_server_entities() { return game_entities_; }

        void Start();

        void RegisterService(::google::protobuf::Service*);

        void LoadGameServerDb();
        void SaveGameServerDb();

        void receive(const common::ServerConnectionES& es);
    private:
        void InitGroupServerDb();        

        muduo::net::RpcServer server_;
        MysqlClientPtr database_;
        std::string nomoral_database_ip_ = "127.0.0.1";
        std::string nomoral_password_ = "luyuan616586";
        std::string redis_ip_ = "127.0.0.1";
        std::string nomoral_ip_ = "127.0.0.1";
        ReuseId reuse_id_;
        GameServerEntities game_entities_;
    };
}//namespace deploy

extern deploy::DeployServer* g_deploy_server;

#endif // !DEPLOY_SERVER_SRCDEPLOY_SERVER_H_
