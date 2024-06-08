#include "game_server.h"

#include "muduo/base/Logging.h"

#include "all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/lobby_config.h"

#include "src/event_handler/event_handler.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/gate_node.h"
#include "src/network/process_info.h"
#include "service/centre_service_service.h"
#include "src/handler/player_service.h"
#include "src/replied_handler/player_service_replied.h"
#include "src/handler/register_handler.h"

#include "src/thread_local/game_thread_local_storage.h"

#include "service/service.h"
#include "src/system/logic/config_system.h"

GameNode* g_game_node = nullptr;

NodeId get_gate_node_id()
{
    return g_game_node->gs_info().id();
}

void InitRepliedHandler();

GameNode::GameNode(muduo::net::EventLoop* loop)
    :loop_(loop),
     redis_(std::make_shared<PbSyncRedisClientPtr::element_type>()){}

GameNode::~GameNode()
{
	tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&GameNode::Receive1>(*this);
	tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&GameNode::Receive2>(*this);
}

void GameNode::Init()
{
    g_game_node = this; 
    EventHandler::Register();
    InitConfig();
	node_info_.set_node_type(kGameNode);
	node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());
    muduo::Logger::setLogLevel((muduo::Logger::LogLevel)ZoneConfig::GetSingleton().config_info().loglevel());
    global_entity();
    tls.registry.emplace<GsServerType>(global_entity(), GsServerType{ ZoneConfig::GetSingleton().config_info().server_type() });
    LOG_INFO << "server type" << ZoneConfig::GetSingleton().config_info().server_type();
    InitMessageInfo();
    InitPlayerService();
    InitPlayerServiceReplied();
    InitRepliedHandler();
    InitNetwork();

	void InitServiceHandler();
	InitServiceHandler();
}

void GameNode::InitConfig()
{
	ZoneConfig::GetSingleton().Load("game.json");
	DeployConfig::GetSingleton().Load("deploy.json");
	LobbyConfig::GetSingleton().Load("lobby.json");
    LoadAllConfigAsyncWhenServerLaunch();
    ConfigSystem::OnConfigLoadSuccessful();
}

void GameNode::InitNetwork()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_node_ = std::make_unique<RpcClient>(loop_, deploy_addr);
	tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&GameNode::Receive1>(*this);
    deploy_node_->connect();
}

void GameNode::ServerInfo(const ::servers_info_data& info)
{
    InetAddress serverAddr(info.redis_info().ip(), info.redis_info().port());
    game_tls.redis_system().Init(serverAddr);
}

void GameNode::CallCentreStartGs(CentreSessionPtr controller_node)
{
    auto& controller_local_addr = controller_node->local_addr();
    CtrlStartGsRequest rq;
    auto session_info = rq.mutable_rpc_client();
    auto node_info = rq.mutable_rpc_server();
    session_info->set_ip(controller_local_addr.toIp());
    session_info->set_port(controller_local_addr.port());
    node_info->set_ip(gs_info_.ip());
    node_info->set_port(gs_info_.port());
    rq.set_server_type(tls.registry.get<GsServerType>(global_entity()).server_type_);
    rq.set_gs_node_id(gs_info_.id());
    controller_node->CallMethod(CentreServiceStartGsMsgId,rq);
    LOG_DEBUG << "connect to controller" ;
}

void GameNode::CallLobbyStartGs()
{
}

void GameNode::Receive1(const OnConnected2ServerEvent& es)
{
    auto& conn = es.conn_;
    if (deploy_node_->peer_addr().toIpPort() == conn->peerAddress().toIpPort())
    {
        // started 
        if (nullptr != server_)
        {
            return;
        }
    }

    for (auto& it : game_tls.centre_node())
    {
        auto& controller_node = it.second;
        auto& controller_session = controller_node->session_;
        if (conn->connected() &&
            IsSameAddr(controller_session->peer_addr(), conn->peerAddress()))
        {
            EventLoop::getEventLoopOfCurrentThread()->queueInLoop(std::bind(&GameNode::CallCentreStartGs, this, controller_session));
            break;
        }
        // ms 走断线重连，不删除
    }

    if (nullptr != lobby_node_)
    {
		if (conn->connected() && 
            IsSameAddr(lobby_node_->peer_addr(), conn->peerAddress()))
		{
			EventLoop::getEventLoopOfCurrentThread()->queueInLoop(std::bind(&GameNode::CallLobbyStartGs, this));
		}
		else if (!conn->connected() &&
			IsSameAddr(lobby_node_->peer_addr(), conn->peerAddress()))
		{

		}
    }
  
}

void GameNode::Receive2(const OnBeConnectedEvent& es)
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
			auto gatenode = tls.registry.try_get<GateNodePtr>(e);//如果是gate
			if (nullptr != gatenode && (*gatenode)->node_info_.node_type() == kGateNode)
			{
                game_tls.gate_node().erase((*gatenode)->node_info_.node_id());
			}
			tls.registry.destroy(e);
			break;
		}
    }
}

void GameNode::Connect2Lobby()
{
    lobby_node_->registerService(&gs_service_impl_);
    lobby_node_->connect();
}