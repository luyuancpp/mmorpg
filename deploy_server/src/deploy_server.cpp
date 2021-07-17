#include "deploy_server.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>

#include "muduo/base/Logging.h"
#include "muduo/net/Callbacks.h"

#include "src/game_config/game_config.h"
#include "src/server_common/rpc_channel.h"

#include "deploy_database_table.pb.h"

using namespace muduo;
using namespace muduo::net;
using namespace std::placeholders;

static const std::string kIp("127.0.0.1");

namespace deploy_server
{
    DeployServer::DeployServer(muduo::net::EventLoop* loop,
        const muduo::net::InetAddress& listen_addr)
        :server_(loop, listen_addr, "DeployServer"),
        database_(std::make_shared<common::MysqlDatabase>())
    {
        database_->Connect(common::DeployConfig::GetSingleton().connetion_param());

        server_.setConnectionCallback(
            std::bind(&DeployServer::onConnection, this, _1));
        //   server_.setMessageCallback(
        //       std::bind(&RpcServer::onMessage, this, _1, _2, _3));
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
        const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
        services_[desc->full_name()] = service;
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


    void DeployServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
    {
        LOG_INFO << "RpcServer - " << conn->peerAddress().toIpPort() << " -> "
            << conn->localAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
        if (conn->connected())
        {
            RpcChannelPtr channel(new RpcChannel(conn));
            channel->setServices(&services_);
            conn->setMessageCallback(
                std::bind(&RpcChannel::onMessage, get_pointer(channel), _1, _2, _3));
            conn->setContext(channel);
        }
        else
        {
            conn->setContext(RpcChannelPtr());
            // FIXME:
        }
    }
}//namespace deploy_server