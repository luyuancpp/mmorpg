#include "lobby_server.h"

#include "muduo/base/Logging.h"

#include "src/game_config/deploy_json.h"
#include "src/game_config/mainscene_config.h"
#include "src/game_config/lobby_config.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/server_component.h"
#include "src/network/gs_node.h"
#include "src/network/controller_node.h"
#include "src/pb/pbc/service_method/deploy_servicemethod.h"
#include "src/service/common_proto_replied/replied_dispathcer.h"


LobbyServer* g_lobby_server = nullptr;


LobbyServer::LobbyServer(muduo::net::EventLoop* loop): loop_(loop){}

void LobbyServer::Init()
{
    g_lobby_server = this;

    LobbyConfig::GetSingleton().Load("lobby.json");
    DeployConfig::GetSingleton().Load("deploy.json");
    muduo::Logger::setLogLevel((muduo::Logger::LogLevel)LobbyConfig::GetSingleton().config_info().loglevel());
    mainscene_config::GetSingleton().load();
    InitRepliedCallback();
    ConnectDeploy();
}

void LobbyServer::ConnectDeploy()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_session_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_session_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_session_->connect();
}

void LobbyServer::StartServer(const ::lobby_server_db& info)
{
    InetAddress lobby_addr(info.ip(), info.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, lobby_addr);
    server_->registerService(&impl_);
    server_->subscribe<OnBeConnectedEvent>(*this);
    server_->start();
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
            LobbyServerRequest rq;
            rq.set_lobby_id(LobbyConfig::GetSingleton().config_info().lobby_id());
            deploy_session_->CallMethod(DeployServiceStartLobbyServerMethodDesc, &rq);
        }
			
        {
            SceneSqueueRequest rq;
            deploy_session_->CallMethod(DeployServiceSceneSqueueNodeIdMethodDesc, &rq);
        }
	}
      
}

void LobbyServer::receive(const OnBeConnectedEvent& es)
{
	auto& conn = es.conn_;
    if (conn->connected())
    {
        auto e = tls.registry.create();
        tls.registry.emplace<RpcServerConnection>(e, RpcServerConnection{ conn });
    }
    else
    {
		auto& peer_addr = conn->peerAddress();
		for (auto e : tls.registry.view<RpcServerConnection>())
		{
			auto& local_addr = tls.registry.get<RpcServerConnection>(e).conn_->peerAddress();
			if (local_addr.toIpPort() != peer_addr.toIpPort())
			{
				continue;
			}
			auto gsnode = tls.registry.try_get<GsNodePtr>(e);//如果是游戏逻辑服则删除
			if (nullptr != gsnode && (*gsnode)->node_info_.node_type() == kGameNode)
			{
				g_game_node->erase((*gsnode)->node_info_.node_id());
			}
            // controller 不动态扩展，所以不删除
			tls.registry.destroy(e);
			break;
		}
    }		
}
