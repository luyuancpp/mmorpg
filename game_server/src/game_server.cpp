#include "game_server.h"

#include "src/game_config/game_config.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_type_id/server_type_id.h"

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
    deploy_rpc_client_->emp()->subscribe<common::RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->emp()->subscribe<common::ConnectionES>(*this);
    deploy_rpc_client_->connect();
}

void GameServer::receive(const common::ConnectionES& es)
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
        &GameServer::StartServer,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}

void GameServer::StartServer(ServerInfoRpcRC cp)
{
    auto& masterinfo = cp->s_resp_->info(common::SERVER_MASTER);
    InetAddress master_addr(masterinfo.ip(), masterinfo.port());
    master_rpc_client_ = std::make_unique<common::RpcClient>(loop_, master_addr);
    master_rpc_client_->connect();
    master_rpc_client_->emp()->subscribe<common::RegisterStubES>(g2ms_stub_);
}

}//namespace game