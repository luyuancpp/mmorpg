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
            serverinfo_database sd;
            sd.set_ip(kIp);
            sd.set_db_host(kIp);
            sd.set_db_user("root");
            sd.set_db_password("luyuan616586");
            sd.set_db_port(3306);
            sd.set_db_dbname("game");
            for (uint32_t i = 0; i < kTotalSize; ++i)
            {
                sd.set_port(kBeginPort + i);
                if (i % common::SERVER_ID_GROUP_SIZE == 0)
                {
                    sd.set_port(kRedisPort);
                }
                database_->SaveOne(sd);
            }
        }
    }

}//namespace deploy_server