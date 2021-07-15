#include "master_server.h"

#include "src/database/rpcclient/database_rpcclient.h"
#include "src/net/deploy/rpcclient/deploy_rpcclient.h"
#include "src/server_type_id/server_type_id.h"

namespace master
{
MasterServer::MasterServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<common::RedisClient>())
{
    deploy::DeployRpcClient::GetSingleton()->emp()->subscribe<common::ConnectionEvent>(*this);
}    

void MasterServer::Start()
{
    server_->registerService(&impl_);
    server_->start();
}

void MasterServer::receive(const common::ConnectionEvent& es)
{
    if (!es.conn_->connected())
    {
        return;
    }
    ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
    cp->s_reqst_.set_group(1);
    deploy::ServerInfoRpcStub::GetSingleton().CallMethod(
        &MasterServer::StartServer,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}

void MasterServer::StartServer(ServerInfoRpcRC cp)
{
    auto& databaseinfo = cp->s_resp_->info(common::SERVER_DATABASE);
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    master::DbRpcClient::Connect(loop_, database_addr);

    auto& myinfo = cp->s_resp_->info(common::SERVER_MASTER);
    InetAddress login_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, login_addr);

    Start();
}

}//namespace master