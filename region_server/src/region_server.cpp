#include "region_server.h"

#include "muduo/base/Logging.h"

#include "src/game_config/region_config.h"
#include "src/game_config/deploy_json.h"

#include "src/server_common/server_type_id.h"

region::RegionServer* g_region_server = nullptr;

namespace region
{
    RegionServer::RegionServer(muduo::net::EventLoop* loop)
        : loop_(loop)
    {

    }

    void RegionServer::Init()
    {
        common::RegionConfig::GetSingleton().Load("region.json");
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
        auto& myinfo = cp->s_resp_->info();
        InetAddress region_addr(myinfo.ip(), myinfo.port());
        server_ = std::make_shared<muduo::net::RpcServer>(loop_, region_addr);
        server_->subscribe<common::ServerConnectionES>(*this);
        //LOG_INFO << myinfo.DebugString().c_str();
        server_->start();
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
        cp->s_reqst_.set_region_id(common::RegionConfig::GetSingleton().config_info().region_id());
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

