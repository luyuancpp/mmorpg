#include "lobby_server.h"

#include "muduo/base/Logging.h"

#include "deploy_json.h"
#include "mainscene_config.h"
#include "lobby_config.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/server_component.h"
#include "src/network/game_node.h"
#include "src/pb/pbc/deploy_service_service.h"
#include "src/service/replied_dispathcer.h"
#include "src/thread_local/lobby_thread_local_storage.h"


LobbyServer* g_lobby_server = nullptr;


LobbyServer::LobbyServer(muduo::net::EventLoop* loop): loop_(loop){}

LobbyServer::~LobbyServer()
{
	tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&LobbyServer::Receive1>(*this);
	tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&LobbyServer::Receive2>(*this);
}

void LobbyServer::Init()
{
    g_lobby_server = this;

	void InitMessageInfo();
	InitMessageInfo();

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
    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&LobbyServer::Receive1>(*this);
    deploy_session_->connect();
}

void LobbyServer::StartServer(const ::lobby_server_db& info)
{
    InetAddress lobby_addr(info.ip(), info.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, lobby_addr);
    server_->registerService(&impl_);
    tls.dispatcher.sink<OnBeConnectedEvent>().connect<&LobbyServer::Receive2>(*this);
    server_->start();
}


void LobbyServer::Receive1(const OnConnected2ServerEvent& es) const
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
            deploy_session_->CallMethod(DeployServiceStartLobbyServerMsgId, rq);
        }
			
        {
            SceneSqueueRequest rq;
            deploy_session_->CallMethod(DeployServiceSceneSequenceNodeIdMsgId, rq);
        }
	}
      
}

void LobbyServer::Receive2(const OnBeConnectedEvent& es)const
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
			auto game_node = tls.registry.try_get<GameNodePtr>(e);//如果是游戏逻辑服则删除
			if (nullptr != game_node && (*game_node)->node_info_.node_type() == kGameNode)
			{
				lobby_tls.game_node_list().erase((*game_node)->node_info_.node_id());
			}
            // controller 不动态扩展，所以不删除
			tls.registry.destroy(e);
			break;
		}
    }		
}
