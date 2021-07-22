#include "master_server.h"

#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/server_type_id.h"
#include "src/game_config/game_config.h"
#include "src/game_ecs/game_registry.h"

#include "ms2g.pb.h"
#include "ms2gw.pb.h"

using common::reg;

namespace master
{
MasterServer::MasterServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<common::RedisClient>()),
      g2ms_impl_(this)
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
    deploy_rpc_client_->subscribe<common::RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->subscribe<common::ClientConnectionES>(*this);
    deploy_rpc_client_->connect();
}

void MasterServer::receive(const common::ClientConnectionES& es)
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

void MasterServer::StartServer(ServerInfoRpcRC cp)
{
    info_ = cp->s_resp_->info();
    auto& databaseinfo = info_.Get(common::SERVER_DATABASE);
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_rpc_client_ = std::make_unique<common::RpcClient>(loop_, database_addr);
    db_rpc_client_->subscribe<common::RegisterStubES>(msl2_login_stub_);
    db_rpc_client_->connect();    

    auto& myinfo = cp->s_resp_->info(common::SERVER_MASTER);
    InetAddress master_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, master_addr);
    server_->subscribe<common::ServerConnectionES>(*this);

    server_->registerService(&l2ms_impl_);
    server_->registerService(&g2ms_impl_);
    server_->start();
}

void MasterServer::GatewayConnectGame(const InetAddress& peer_addr)
{
    if (nullptr == gate_client_ || !gate_client_->Connected())
    {
        common::WaitingGatewayConnecting wgc{ peer_addr };
        auto e = reg().create();
        reg().emplace<common::WaitingGatewayConnecting>(e, wgc);
        return;
    }
    ms2gw::StartLogicServerRequest request;
    request.set_ip(peer_addr.toIp());
    request.set_port(peer_addr.port());
    gate_client_->Send(request, "ms2gw.Ms2gwService", "StartLogicServer");

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
        if (local_addr.toIp() != peer_addr.toIp() ||
            local_addr.port() != peer_addr.port())
        {
            continue;
        }
        reg().destroy(e);
        break;
    }

}

}//namespace master