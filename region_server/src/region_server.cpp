#include "region_server.h"

#include "src/game_config/deploy_json.h"

region::RegionServer* g_region_server = nullptr;

namespace region
{
    RegionServer::RegionServer(muduo::net::EventLoop* loop)
        : loop_(loop)
    {

    }

    void RegionServer::Init()
    {
        common::GameConfig::GetSingleton().Load("game.json");
        common::DeployConfig::GetSingleton().Load("deploy.json");
    }

    void RegionServer::ConnectDeploy()
    {
        const auto& deploy_info = common::DeployConfig::GetSingleton().deploy_param();
        InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
        deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
        deploy_rpc_client_->subscribe<common::RegisterStubES>(deploy_stub_);
        deploy_rpc_client_->subscribe<common::RpcClientConnectionES>(*this);
        deploy_rpc_client_->connect();
    }

    void RegionServer::StartServer(RegionInfoRpcRpcRC cp)
    {

    }

    void RegionServer::receive(const common::RpcClientConnectionES& es)
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
        RegionInfoRpcRpcRC cp(std::make_shared<RegionInfoRpcClosure>());
        cp->s_reqst_.set_region_id(common::GameConfig::GetSingleton().config_info().region_id());
        deploy_stub_.CallMethod(
            &RegionServer::StartServer,
            cp,
            this,
            &deploy::DeployService_Stub::StartRegionServer);
    }

    void RegionServer::receive(const common::ServerConnectionES& es)
    {
    }
}

