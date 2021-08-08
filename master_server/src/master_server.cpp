#include "master_server.h"

#include "muduo/base/Logging.h"

#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/server_type_id.h"
#include "src/game_config/deploy_json.h"
#include "src/game_logic/game_registry.h"
#include "src/game/game_client.h"

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
    deploy_rpc_client_->subscribe<common::RpcClientConnectionES>(*this);
    deploy_rpc_client_->connect();
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

void MasterServer::StartServer(ServerInfoRpcRC cp)
{
    serverinfo_database_ = cp->s_resp_->info();
    auto& databaseinfo = serverinfo_database_.Get(common::SERVER_DATABASE);
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
    server_->registerService(&gw2ms_impl_);
    server_->start();
}

void MasterServer::GatewayConnectGame(const common::WaitingGatewayConnecting& connection_info)
{
    if (nullptr == gate_client_ || !gate_client_->Connected())
    {
        LOG_INFO << "gate off line";
        return;
    }
    /* ms2gw::StartLogicServerRequest request;
     request.set_ip(connection_info.addr_.toIp());
     request.set_port(connection_info.addr_.port());
     request.set_server_id(connection_info.server_id_);
     gate_client_->Send(request, "ms2gw.Ms2gwService", "StartLogicServer");*/
}


void MasterServer::OnRpcClientConnectionConnect(const muduo::net::TcpConnectionPtr& conn)
{
    auto e = reg().create();
    auto& rpc_client =  reg().emplace<common::RpcServerConnection>(e, common::RpcServerConnection{ conn });
    if (!IsGroupServer(conn->peerAddress()))
    {
        ms2gw::StartLogicServerRequest request;
        request.set_ip("127.0.0.1");
        request.set_port(888);
        request.set_server_id(0);
        rpc_client.Send(request, "ms2gw.Ms2gwService", "StartLogicServer");

    }
}

void MasterServer::OnRpcClientConnectionDisConnect(const muduo::net::TcpConnectionPtr& conn)
{
    auto& peer_addr = conn->peerAddress();

    for (auto e : GameClient::GetSingleton()->view<common::RpcServerConnection>())
    {
        auto& local_addr = GameClient::GetSingleton()->get<common::RpcServerConnection>(e).conn_->peerAddress();
        if (local_addr.toIp() != peer_addr.toIp() ||
            local_addr.port() != peer_addr.port())
        {
            continue;
        }
        GameClient::GetSingleton()->destroy(e);
        break;
    }

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

bool MasterServer::IsGroupServer(const InetAddress& peer_addr)
{
    for (uint32_t i = common::SERVER_DATABASE; i < common::SERVER_CURENT_USER; ++ i)
    {
        auto& server_data = serverinfo_database_.Get(int32_t(i));
        if (peer_addr.toIp() == server_data.ip() && peer_addr.port() == server_data.port())
        {
            return true;
        }
    }
    return false;
}

}//namespace master