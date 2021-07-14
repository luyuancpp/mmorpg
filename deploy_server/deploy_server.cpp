#include "deploy_server.h"

#include "src/config/deploy_config.h"
#include "src/server_type_id/server_type_id.h"

#include "deploy_database_table.pb.h"

namespace deploy_server
{
    DeployServer::DeployServer(muduo::net::EventLoop* loop,
        const muduo::net::InetAddress& listen_addr)
        :server_(loop, listen_addr),
        database_(std::make_shared<common::MysqlDatabase>())
    {
        database_->Connect(DeployConfig::GetSingleton().connetion_param());
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
            uint32_t group_server_size = common::SERVER_ID_GROUP_SIZE - common::SERVER_REDIS;
            uint32_t begin_port = 2000;
            uint32_t group_size = 500 * group_server_size;
            serverinfo_database sd;
            for (uint32_t i = 0; i < group_size; ++i)
            {
                sd.set_port(begin_port + i);
                sd.set_ip("127.0.0.1");
                database_->SaveOne(sd);
            }
        }
    }

}//namespace deploy_server