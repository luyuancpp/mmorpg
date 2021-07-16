#include "master_server.h"

#include "src/server_common/deploy_rpcclient.h"
#include "src/server_type_id/server_type_id.h"

#include "src/game_config/game_config.h"

namespace master
{
MasterServer::MasterServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<common::RedisClient>())
{ 

}    

void MasterServer::LoadConfig()
{
    common::GameConfig::GetSingleton().Load("game.json");
    common::DeployConfig::GetSingleton().Load("deploy.json");
}

void MasterServer::ConnectDeploy()
{
    const auto& deploy_info = common::DeployConfig::GetSingleton().deploy_param();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->emp()->subscribe<common::RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->emp()->subscribe<common::ConnectionES>(*this);
    deploy_rpc_client_->connect();
}

void MasterServer::Start()
{
    server_->registerService(&impl_);
    server_->start();
}

void MasterServer::receive(const common::ConnectionES& es)
{
    if (!es.conn_->connected())
    {
        return;
    }
    ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
    cp->s_reqst_.set_group(common::GameConfig::GetSingleton().config_info().group_id());
    deploy_stub_.CallMethod(
        &MasterServer::StartServer,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}

void MasterServer::StartServer(ServerInfoRpcRC cp)
{
    auto& databaseinfo = cp->s_resp_->info(common::SERVER_DATABASE);
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_rpc_client_ = std::make_unique<common::RpcClient>(loop_, database_addr);
    db_rpc_client_->connect();
    db_rpc_client_->emp()->subscribe<common::RegisterStubES>(msl2_login_stub_);

    auto& myinfo = cp->s_resp_->info(common::SERVER_MASTER);
    InetAddress master_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, master_addr);
    
    Start();
}

}//namespace master