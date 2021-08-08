#include "game_server.h"

#include "src/game_config/deploy_json.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/deploy_variable.h"
#include "src/server_common/server_type_id.h"

#include "muduo/base/CrossPlatformAdapterFunction.h"

game::GameServer* g_game_server = nullptr;

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

void GameServer::InitNetwork()
{
    const auto& deploy_info = common::DeployConfig::GetSingleton().deploy_param();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->subscribe<common::RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->subscribe<common::RpcClientConnectionES>(*this);
    deploy_rpc_client_->connect();
}

void GameServer::receive(const common::RpcClientConnectionES& es)
{
    if (!es.conn_->connected())
    {
        return;
    }
   
    if (deploy_rpc_client_->peer_addr().toIp() == es.conn_->peerAddress().toIp() && 
        deploy_rpc_client_->peer_addr().port() == es.conn_->peerAddress().port())
    {
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
    if (nullptr != master_rpc_client_ && 
        deploy_rpc_client_->peer_addr().toIp() == es.conn_->peerAddress().toIp())
    {
        Register2Master();
    }
}

void GameServer::ServerInfo(ServerInfoRpcRC cp)
{
    auto& masterinfo = cp->s_resp_->info(common::SERVER_MASTER);
    InetAddress master_addr(masterinfo.ip(), masterinfo.port());
    master_rpc_client_ = std::make_unique<common::RpcClient>(loop_, master_addr);
    
    StartGameServerRpcRC scp(std::make_shared<StartGameServerInfoRpcClosure>());
    scp->s_reqst_.set_group(common::GameConfig::GetSingleton().config_info().group_id());
    scp->s_reqst_.mutable_my_info()->set_ip(muduo::ProcessInfo::localip());
    deploy_stub_.CallMethod(
        &GameServer::StartGameServer,
        scp,
        this,
        &deploy::DeployService_Stub::StartGameServer);
}

void GameServer::StartGameServer(StartGameServerRpcRC cp)
{
    //uint32_t snid = server_info_.id() - deploy_server::kGameSnowflakeIdReduceParam;//snowflake id 
    master_rpc_client_->subscribe<common::RegisterStubES>(g2ms_stub_);
    master_rpc_client_->registerService(&ms2g_service_impl_);
    master_rpc_client_->subscribe<common::RpcClientConnectionES>(*this);
    master_rpc_client_->connect();

    server_info_ = cp->s_resp_->my_info();
    InetAddress game_addr(server_info_.ip(), server_info_.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, game_addr);
    server_->start();   
}

void GameServer::Register2Master()
{
    auto& master_local_addr = master_rpc_client_->local_addr();
    g2ms::StartGameServerRequest request;
        auto rpc_client = request.mutable_rpc_client();
        rpc_client->set_ip(master_local_addr.toIp());
        rpc_client->set_port(master_local_addr.port());
        request.set_server_id(server_info_.id());
        g2ms_stub_.CallMethod(
            request,
            &g2ms::G2msService_Stub::StartGameServer);
}

}//namespace game
