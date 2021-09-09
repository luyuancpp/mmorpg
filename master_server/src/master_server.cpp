#include "master_server.h"

#include "muduo/base/Logging.h"


#include "src/game_config/all_config.h"

#include "src/factories/scene_factories.hpp"
#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/comp/player.hpp"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/server_type_id.h"
#include "src/game_config/deploy_json.h"
#include "src/game_logic/game_registry.h"

#include "ms2g.pb.h"
#include "ms2gw.pb.h"

using common::reg;

master::MasterServer* g_master_server = nullptr;

namespace master
{
MasterServer::MasterServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<common::RedisClient>()),
      g2ms_impl_(),
      gw2ms_impl_()
{ 
}    

void MasterServer::Init()
{
    InitConfig();
    InitGlobalEntities();
}

void MasterServer::ConnectDeploy()
{
    const auto& deploy_info = common::DeployConfig::GetSingleton().deploy_param();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->subscribe<common::RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->subscribe<common::RpcClientConnectionES>(*this);
    deploy_rpc_client_->connect();
}

void MasterServer::StartServer(ServerInfoRpcRC cp)
{
    serverinfo_database_ = cp->s_resp_->info();
    auto& databaseinfo = serverinfo_database_.Get(common::kServerDatabase);
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_rpc_client_ = std::make_unique<common::RpcClient>(loop_, database_addr);
    db_rpc_client_->subscribe<common::RegisterStubES>(msl2_login_stub_);
    db_rpc_client_->connect();    

    auto& myinfo = cp->s_resp_->info(common::kServerMaster);
    InetAddress master_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, master_addr);
    server_->subscribe<common::ServerConnectionES>(*this);

    server_->registerService(&l2ms_impl_);
    server_->registerService(&g2ms_impl_);
    server_->registerService(&gw2ms_impl_);
    server_->start();
}

void MasterServer::GatewayConnectGame(entt::entity ge)
{
    if (nullptr == gate_client_ || !gate_client_->Connected())
    {
        return;
    }
    auto connection_info = reg().try_get<InetAddress>(ge);
    if (nullptr == connection_info)
    {
        return;
    }
    ms2gw::StartGameServerRequest request;
    request.set_ip(connection_info->toIp());
    request.set_port(connection_info->port());
    request.set_server_id(reg().get<common::GameServerDataPtr>(ge)->server_id());
    gate_client_->Send(request, "ms2gw.Ms2gwService", "StartGameServer");
}

void MasterServer::receive(const common::RpcClientConnectionES& es)
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
        &MasterServer::StartServer,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}

void MasterServer::receive(const common::ServerConnectionES& es)
{
    auto& conn = es.conn_;
    if (conn->connected())
    {
        OnRpcClientConnectionConnect(conn);
    }
    else
    {
        OnRpcClientConnectionDisConnect(conn);
    }
}

void MasterServer::OnRpcClientConnectionConnect(const muduo::net::TcpConnectionPtr& conn)
{
    auto e = reg().create();
    reg().emplace<common::RpcServerConnection>(e, common::RpcServerConnection{ conn });
}

void MasterServer::OnRpcClientConnectionDisConnect(const muduo::net::TcpConnectionPtr& conn)
{
    auto& peer_addr = conn->peerAddress();
    for (auto e : reg().view<common::RpcServerConnection>())
    {
        auto& local_addr = reg().get<common::RpcServerConnection>(e).conn_->peerAddress();
        if (local_addr.toIpPort() != peer_addr.toIpPort())
        {
            continue;
        }
        reg().destroy(e);
        break;
    }
}

void MasterServer::InitConfig()
{
    common::GameConfig::GetSingleton().Load("game.json");
    common::DeployConfig::GetSingleton().Load("deploy.json");
    loadallconfig();
}

void MasterServer::InitGlobalEntities()
{
    MakeScenes();
    global_entity() = reg().create();
    reg().emplace<common::ConnectionPlayerEnitiesMap>(global_entity());
}

}//namespace master