#include "deploy_server.h"

#include "src/game_config/game_config.h"
#include "src/server_type_id/server_type_id.h"

#include "deploy_database_table.pb.h"

static const uint32_t kRedisPort = 6379;
static const uint32_t kGroup = 500;
static const uint32_t kBeginPort = 2000;
static const uint32_t kGroupServerSize = common::SERVER_ID_GROUP_SIZE - common::SERVER_REDIS + 1;
static const uint32_t kTotalSize = kGroup * kGroupServerSize;
static const std::string kIp = "127.0.0.1";

namespace deploy_server
{
    DeployServer::DeployServer(muduo::net::EventLoop* loop,
        const muduo::net::InetAddress& listen_addr)
        :server_(loop, listen_addr),
        database_(std::make_shared<common::MysqlDatabase>())
    {
        database_->Connect(common::DeployConfig::GetSingleton().connetion_param());
    }

    void DeployServer::Start()
    {
        database_->AddTable(serverinfo_database::default_instance());
        database_->Init();
        InitServerInof();
        server_.start();
    }

    void DeployServer::RegisterService(google::protobuf::Service* service)
    {
        server_.registerService(service);
    }

    void DeployServer::InitServerInof()
    {
        auto q_result = database_->QueryOne("select * from serverinfo_database LIMIT 1");
        if (nullptr == q_result)
        {
            serverinfo_database sd_db;
            sd_db.set_ip(kIp);
            sd_db.set_db_host(kIp);
            sd_db.set_db_user("root");
            sd_db.set_db_password("luyuan616586");
            sd_db.set_db_port(3306);
            sd_db.set_db_dbname("game");

            serverinfo_database sd_nodb;
            sd_nodb.set_ip(kIp);

            for (uint32_t i = 0; i < kTotalSize; ++i)
            {
                sd_db.set_port(kBeginPort + i);
                if (i % common::SERVER_ID_GROUP_SIZE == 0)
                {
                    sd_db.set_port(kRedisPort);
                }
                sd_nodb.set_port(sd_db.port());
                if (i % common::SERVER_ID_GROUP_SIZE == common::SERVER_DATABASE)
                {
                    database_->SaveOne(sd_db);
                }
                else
                {
                    database_->SaveOne(sd_nodb);
                }
                
            }
        }
    }

}//namespace deploy_server