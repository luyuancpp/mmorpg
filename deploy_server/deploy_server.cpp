#include "deploy_server.h"

#include "src/config/deploy_config.h"

#include "deploy.pb.h"

namespace deploy_server
{
    DeployServer::DeployServer(muduo::net::EventLoop* loop,
        const muduo::net::InetAddress& listen_addr)
        :server_(loop, listen_addr),
        database_(std::make_shared<common::MysqlDatabase>()),
        redis_(std::make_shared<common::RedisClient>())
    {
        redis_->Connect(listen_addr.toIp(), 1, 1);
        DeployConfig dcfg;
        dcfg.Load("deploy.json");
        database_->Connect(dcfg.connetion_param());
    }

    void DeployServer::Start()
    {
        database_->AddTable(serverinfo_database::default_instance());
        database_->Init();
        server_.start();
    }

    void DeployServer::RegisterService(google::protobuf::Service* service)
    {
        server_.registerService(service);
    }
}//namespace deploy_server