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
    RegionServer::RegionServer(muduo::net::EventLoop* loop): loop_(loop){}

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

    void RegionServer::StartServer(RegionInfoRpcRpcRpc replied)
    {
        auto& myinfo = replied->s_rp_->info();
        InetAddress region_addr(myinfo.ip(), myinfo.port());
        server_ = std::make_shared<RpcServerPtr::element_type>(loop_, region_addr);
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
			RegionInfoRpcRpcRpc rpc(std::make_shared<RegionInfoRpcRpcRpc::element_type>());
			rpc->s_rq_.set_region_id(RegionConfig::GetSingleton().config_info().region_id());
			deploy_stub_.CallMethod(
				&RegionServer::StartServer,
				rpc,
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
                // ms 不动态扩展，所以不删除
				registry.destroy(e);
				break;
			}
        }		
    }
}

