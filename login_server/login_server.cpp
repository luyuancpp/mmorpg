#include "login_server.h"

#include "src/database/rpcclient/database_rpcclient.h"
#include "src/master/rpcclient/master_rpcclient.h"
#include "src/net/deploy/rpcclient/deploy_rpcclient.h"
#include "src/server_type_id/server_type_id.h"

#include "common.pb.h"

namespace login
{
LoginServer::LoginServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<common::RedisClient>())
{
    deploy::DeployRpcClient::GetSingleton()->emp()->subscribe<common::ConnectionEvent>(*this);
}

void LoginServer::Start()
{
    server_->registerService(&impl_);
    impl_.set_redis_client(redis_client());
    server_->start();
}

void LoginServer::receive(const common::ConnectionEvent& es)
{
    if (!es.conn_->connected())
    {
        return;
    }
    ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
    cp->s_reqst_.set_group(1);
    deploy::ServerInfoRpcStub::GetSingleton().CallMethod(
        &LoginServer::StartServer,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}

void LoginServer::StartServer(ServerInfoRpcRC cp)
{
    auto& databaseinfo = cp->s_resp_->info(common::SERVER_DATABASE);
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    login::DbRpcClient::Connect(loop_, database_addr);
    login::DbLoginRpcStub::GetSingleton();

    auto& masterinfo = cp->s_resp_->info(common::SERVER_MASTER);
    InetAddress master_addr(masterinfo.ip(), masterinfo.port());
    login::MasterRpcClient::Connect(loop_, master_addr);
    login::MasterLoginRpcStub::GetSingleton();

    auto& redisinfo = cp->s_resp_->info(common::SERVER_REDIS);
    redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);
 
    auto& myinfo = cp->s_resp_->info(common::SERVER_LOGIN);
    InetAddress login_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, login_addr);
   
    Start();
}

}


