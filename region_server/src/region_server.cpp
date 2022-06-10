#include "region_server.h"

#include "muduo/base/Logging.h"

#include "src/game_config/region_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_logic/game_registry.h"
#include "src/network/server_component.h"
#include "src/network/gs_node.h"
#include "src/network/ms_node.h"

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
        g_region_server = this;

        RegionConfig::GetSingleton().Load("region.json");
        DeployConfig::GetSingleton().Load("deploy.json");

        ConnectDeploy();
    }

    void RegionServer::ConnectDeploy()
    {
        const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
        InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
        deploy_rpc_client_ = std::make_unique<RpcClient>(loop_, deploy_addr);
        deploy_rpc_client_->subscribe<RegisterStubEvent>(deploy_stub_);
        deploy_rpc_client_->subscribe<OnConnected2ServerEvent>(*this);
        deploy_rpc_client_->connect();
    }

    void RegionServer::StartServer(RegionInfoRpcRpcRC cp)
    {
        auto& myinfo = cp->s_rp_->info();
        InetAddress region_addr(myinfo.ip(), myinfo.port());
        server_ = std::make_shared<muduo::net::RpcServer>(loop_, region_addr);
        server_->registerService(&impl_);
        server_->subscribe<OnBeConnectedEvent>(*this);
        server_->start();
    }

    void RegionServer::receive(const OnConnected2ServerEvent& es)
    {
        // started 
        if (nullptr != server_)
        {
            return;
        }
		if (es.conn_->connected())
		{
			RegionInfoRpcRpcRC cp(std::make_shared<RegionInfoRpcClosure>());
			cp->s_rq_.set_region_id(RegionConfig::GetSingleton().config_info().region_id());
			deploy_stub_.CallMethod(
				&RegionServer::StartServer,
				cp,
				this,
				&deploy::DeployService_Stub::StartRegionServer);
		}
      
    }

    void RegionServer::receive(const OnBeConnectedEvent& es)
    {
		auto& conn = es.conn_;
        if (conn->connected())
        {
            auto e = registry.create();
            registry.emplace<RpcServerConnection>(e, RpcServerConnection{ conn });
        }
        else
        {
			auto& peer_addr = conn->peerAddress();
			for (auto e : registry.view<RpcServerConnection>())
			{
				auto& local_addr = registry.get<RpcServerConnection>(e).conn_->peerAddress();
				if (local_addr.toIpPort() != peer_addr.toIpPort())
				{
					continue;
				}
				auto gsnode = registry.try_get<GsNodePtr>(e);//如果是游戏逻辑服则删除
				if (nullptr != gsnode && (*gsnode)->node_info_.node_type() == kGsNode)
				{
					g_gs_nodes->erase((*gsnode)->node_info_.node_id());
				}
				auto msnode = registry.try_get<MsNodePtr>(e);//如果是gate
				if (nullptr != msnode && (*msnode)->node_info_.node_type() == kMasterNode)
				{
                    g_ms_nodes->erase((*msnode)->node_info_.node_id());
				}
				registry.destroy(e);
				break;
			}
        }		
    }
}

