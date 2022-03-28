#include "region_server.h"

#include "muduo/base/Logging.h"

#include "src/game_config/region_config.h"
#include "src/game_config/deploy_json.h"

using namespace common;

region::RegionServer* g_region_server = nullptr;

namespace region
{
    RegionServer::RegionServer(muduo::net::EventLoop* loop)
        : loop_(loop)
    {

    }

    void RegionServer::Init()
    {
        RegionConfig::GetSingleton().Load("region.json");
        DeployConfig::GetSingleton().Load("deploy.json");
    }

    void RegionServer::ConnectDeploy()
    {
        const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
        InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
        deploy_rpc_client_ = std::make_unique<RpcClient>(loop_, deploy_addr);
        deploy_rpc_client_->subscribe<RegisterStubEvent>(deploy_stub_);
        deploy_rpc_client_->subscribe<OnClientConnectedEvent>(*this);
        deploy_rpc_client_->connect();
    }

    void RegionServer::StartServer(RegionInfoRpcRpcRC cp)
    {
        auto& myinfo = cp->s_rp_->info();
        InetAddress region_addr(myinfo.ip(), myinfo.port());
        server_ = std::make_shared<muduo::net::RpcServer>(loop_, region_addr);
        server_->subscribe<OnBeConnectedEvent>(*this);
        //LOG_INFO << myinfo.DebugString().c_str();
        server_->start();
    }

    void RegionServer::receive(const OnClientConnectedEvent& es)
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
        cp->s_rq_.set_region_id(RegionConfig::GetSingleton().config_info().region_id());
        deploy_stub_.CallMethod(
            &RegionServer::StartServer,
            cp,
            this,
            &deploy::DeployService_Stub::StartRegionServer);
    }

    void RegionServer::receive(const OnBeConnectedEvent& es)
    {
    }
}

