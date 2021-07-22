#include "database_server.h"

#include "src/game_config/game_config.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/rpc_connection_event.h"
#include "src/server_common/server_type_id.h"

#include "mysql_database_table.pb.h"

namespace database
{
    DatabaseServer::DatabaseServer(muduo::net::EventLoop* loop)
        : loop_(loop),
          database_(std::make_shared<common::MysqlDatabase>()),
          redis_(std::make_shared<common::RedisClient>())
    {
    }

    void DatabaseServer::LoadConfig()
    {
        common::GameConfig::GetSingleton().Load("game.json");
        common::DeployConfig::GetSingleton().Load("deploy.json");
    }

    void DatabaseServer::ConnectDeploy()
    {
        const auto& deploy_info = common::DeployConfig::GetSingleton().deploy_param();
        InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
        deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
        deploy_rpc_client_->subscribe<common::RegisterStubES>(deploy_stub_);
        deploy_rpc_client_->subscribe<common::RpcClientConnectionES>(*this); 
        deploy_rpc_client_->connect();
    }

    void DatabaseServer::Start()
    {
        database_->AddTable(account_database::default_instance());
        database_->AddTable(player_database::default_instance());
        database_->Init();

        impl_.set_player_mysql_client(player_mysql_client());
        impl_.set_redis_client(redis_client());
        server_->registerService(&impl_);
        server_->start();
    }

    void DatabaseServer::receive(const common::RpcClientConnectionES& es)
    {
        if (!es.conn_->connected())
        {
            return;
        }
        // started 
        if (nullptr != server_)
        {
            return;
        }
        ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
        cp->s_reqst_.set_group(common::GameConfig::GetSingleton().config_info().group_id());
        deploy_stub_.CallMethod(
            &DatabaseServer::StartServer,
            cp,
            this,
            &deploy::DeployService_Stub::ServerInfo);
    }

    void DatabaseServer::StartServer(ServerInfoRpcRC cp)
    {
        if (cp->s_resp_->info().size() < common::SERVER_ID_GROUP_SIZE)
        {
            LOG_ERROR << "depoly server size";
            return;
        }
        auto& redisinfo = cp->s_resp_->info(common::SERVER_REDIS);
        auto& myinfo = cp->s_resp_->info(common::SERVER_DATABASE);        
        InetAddress listenAddr(myinfo.ip(), myinfo.port());
        redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);
        database_->Connect(myinfo);
        server_ = std::make_shared<muduo::net::RpcServer>(loop_, listenAddr);
        Start();
    }


}//namespace database


