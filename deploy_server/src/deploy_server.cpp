#include "deploy_server.h"

#include "muduo/base/Logging.h"
#include "src/game_config/deploy_json.h"
#include "src/server_common/deploy_variable.h"

#include "deploy_database_table.pb.h"

namespace deploy_server
{
    DeployServer::DeployServer(muduo::net::EventLoop* loop,
        const muduo::net::InetAddress& listen_addr)
        :server_(loop, listen_addr),
        database_(std::make_shared<common::MysqlDatabase>())
    {
        auto& ci = common::DeployConfig::GetSingleton().connetion_param();
        nomoral_server_ip_ = common::DeployConfig::GetSingleton().deploy_param().ip();
        database_->Connect(ci);
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
            sd_db.set_ip(nomoral_server_ip_);
            sd_db.set_db_host(nomoral_server_ip_);
            sd_db.set_db_user("root");
            sd_db.set_db_password("luyuan616586");
            sd_db.set_db_port(3306);
            sd_db.set_db_dbname("game");

            serverinfo_database sd_nodb;
            sd_nodb.set_ip(nomoral_server_ip_);

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