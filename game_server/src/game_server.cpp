#include "game_server.h"

#include "src/game_config/game_config.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/deploy_variable.h"
#include "src/server_common/server_type_id.h"

#include "muduo/base/CrossPlatformAdapterFunction.h"

namespace game
{
GameServer::GameServer(muduo::net::EventLoop* loop)
    :loop_(loop),
     redis_(std::make_shared<common::RedisClient>())
{

}

void GameServer::LoadConfig()
{
    common::GameConfig::GetSingleton().Load("game.json");
    common::DeployConfig::GetSingleton().Load("deploy.json");
}

void GameServer::ConnectDeploy()
{
    const auto& deploy_info = common::DeployConfig::GetSingleton().deploy_param();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->subscribe<common::RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->subscribe<common::ClientConnectionES>(*this);
    deploy_rpc_client_->connect();
}

void GameServer::receive(const common::ClientConnectionES& es)
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
        &GameServer::ServerInfo,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}

void GameServer::ServerInfo(ServerInfoRpcRC cp)
{
    auto& masterinfo = cp->s_resp_->info(common::SERVER_MASTER);
    InetAddress master_addr(masterinfo.ip(), masterinfo.port());
    master_rpc_client_ = std::make_unique<common::RpcClient>(loop_, master_addr);
    
    StartLogicServerRpcRC scp(std::make_shared<StartLogicServerInfoRpcClosure>());
    scp->s_reqst_.set_group(common::GameConfig::GetSingleton().config_info().group_id());
    scp->s_reqst_.mutable_my_info()->set_ip(muduo::ProcessInfo::localip());
    deploy_stub_.CallMethod(
        &GameServer::StartLogicServer,
        scp,
        this,
        &deploy::DeployService_Stub::StartLogicServer);
}

void GameServer::StartLogicServer(StartLogicServerRpcRC cp)
{
    server_info = cp->s_resp_->my_info();
    uint32_t snid = server_info.id() - deploy_server::kLogicSnowflakeIdReduceParam;//snowflake id 

    InetAddress game_addr(server_info.ip(), server_info.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, game_addr);

    server_->start();

    master_rpc_client_->subscribe<common::RegisterStubES>(g2ms_stub_);
    master_rpc_client_->registerService(&ms2g_service_impl_);
    master_rpc_client_->connect();    
}

}//namespace game