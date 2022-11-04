#include "lobby_server.h"

#include "muduo/base/Logging.h"

#include "src/game_config/deploy_json.h"
#include "src/game_config/mainscene_config.h"
#include "src/game_config/lobby_config.h"
#include "src/game_logic/game_registry.h"
#include "src/network/server_component.h"
#include "src/network/gs_node.h"
#include "src/network/controller_node.h"


LobbyServer* g_lobby_server = nullptr;

void set_server_squence_node_id(uint32_t node_id);

LobbyServer::LobbyServer(muduo::net::EventLoop* loop): loop_(loop){}

void LobbyServer::Init()
{
    g_lobby_server = this;

    LobbyConfig::GetSingleton().Load("lobby.json");
    DeployConfig::GetSingleton().Load("deploy.json");
    muduo::Logger::setLogLevel((muduo::Logger::LogLevel)LobbyConfig::GetSingleton().config_info().loglevel());
    mainscene_config::GetSingleton().load();

    ConnectDeploy();
}

void LobbyServer::ConnectDeploy()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->subscribe<RegisterStubEvent>(deploy_stub_);
    deploy_rpc_client_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_rpc_client_->connect();
}

void LobbyServer::StartServer(LobbyInfoRpc replied)
{
    auto& myinfo = replied->s_rp_->info();
    InetAddress lobby_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, lobby_addr);
    server_->registerService(&impl_);
    server_->subscribe<OnBeConnectedEvent>(*this);
    server_->start();
}

void LobbyServer::SceneSqueueNodeId(SceneNodeSequeIdRpc replied)
{
    set_server_squence_node_id(replied->s_rp_->node_id());
}

void LobbyServer::receive(const OnConnected2ServerEvent& es)
{
    // started 
    if (nullptr != server_)
    {
        return;
    }
	if (es.conn_->connected())
	{
        {
            LobbyInfoRpc rpc(std::make_shared<LobbyInfoRpc::element_type>());
            rpc->s_rq_.set_lobby_id(LobbyConfig::GetSingleton().config_info().lobby_id());
            deploy_stub_.CallMethod(
                &LobbyServer::StartServer,
                rpc,
                this,
                &deploy::DeployService_Stub::StartLobbyServer);
        }
			

        {
            SceneNodeSequeIdRpc rpc(std::make_shared<SceneNodeSequeIdRpc::element_type>());
            deploy_stub_.CallMethod(
                &LobbyServer::SceneSqueueNodeId,
                rpc,
                this,
                &deploy::DeployService_Stub::SceneSqueueNodeId);
        }
	}
      
}

void LobbyServer::receive(const OnBeConnectedEvent& es)
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
            // controller 不动态扩展，所以不删除
			registry.destroy(e);
			break;
		}
    }		
}
