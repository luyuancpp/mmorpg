#include "game_server.h"

#include "muduo/base/Logging.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"

#include "src/game_config/all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/lobby_config.h"

#include "src/event_handler/event_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/gate_node.h"
#include "src/pb/pbc/controller_service_service.h"
#include "src/pb/pbc/deploy_service_service.h"
#include "src/pb/pbc/lobby_scene_service.h"
#include "src/handler/player_service.h"
#include "src/replied_handler/player_service_replied.h"
#include "src/handler/register_handler.h"

#include "src/thread_local/game_thread_local_storage.h"

#include "src/pb/pbc/service.h"
#include "src/system/logic/config_system.h"

GameServer* g_game_node = nullptr;

NodeId node_id()
{
    return g_game_node->gs_info().id();
}

void InitRepliedHandler();

GameServer::GameServer(muduo::net::EventLoop* loop)
    :loop_(loop),
     redis_(std::make_shared<PbSyncRedisClientPtr::element_type>()){}

void GameServer::Init()
{
    g_game_node = this; 
    EventHandler::Register();
    InitConfig();
	node_info_.set_node_type(kGameNode);
	node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());
    muduo::Logger::setLogLevel((muduo::Logger::LogLevel)GameConfig::GetSingleton().config_info().loglevel());
    global_entity() = tls.registry.create();
    tls.registry.emplace<GsServerType>(global_entity(), GsServerType{ GameConfig::GetSingleton().config_info().server_type() });
    LOG_INFO << "server type" << GameConfig::GetSingleton().config_info().server_type();
    InitMessageInfo();
    InitPlayerService();
    InitPlayerServiceReplied();
    InitRepliedHandler();
    InitNetwork();
}

void GameServer::InitConfig()
{
	GameConfig::GetSingleton().Load("game.json");
	DeployConfig::GetSingleton().Load("deploy.json");
	LobbyConfig::GetSingleton().Load("lobby.json");
    LoadAllConfigAsyncWhenServerLaunch();
    ConfigSystem::OnConfigLoadSuccessful();
}

void GameServer::InitNetwork()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_node_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_node_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_node_->connect();
}

void GameServer::ServerInfo(const ::servers_info_data& info)
{
    auto& lobby_info = info.lobby_info();
    InetAddress lobby_addr(lobby_info.ip(), lobby_info.port());
    
    lobby_node_ = std::make_unique<RpcClient>(loop_, lobby_addr);
    
    InetAddress serverAddr(info.redis_info().ip(), info.redis_info().port());
    game_tls.redis_system().Init(serverAddr);

    {
        StartGSRequest rq;
        rq.set_group(GameConfig::GetSingleton().config_info().group_id());
        rq.mutable_my_info()->set_ip(muduo::ProcessInfo::localip());
        rq.mutable_my_info()->set_id(gs_info_.id());
        rq.mutable_rpc_client()->set_ip(deploy_node_->local_addr().toIp());
        rq.mutable_rpc_client()->set_port(deploy_node_->local_addr().port());
        deploy_node_->CallMethod(DeployServiceStartGSMsgId, rq);
    }
   
    {
        LobbyServerRequest rq;
        rq.set_lobby_id(LobbyConfig::GetSingleton().config_info().lobby_id());
        deploy_node_->CallMethod(DeployServiceAcquireLobbyInfoMsgId, rq);//获取大厅服下所有服务器信息
    }
	
}

void GameServer::StartGsDeployReplied(const StartGSResponse& replied)
{
    Connect2Lobby();

    auto& redis_info = replied.redis_info();
	redis_->Connect(redis_info.ip(), redis_info.port(), 1, 1);

    gs_info_ = replied.my_info();
    node_info_.set_node_id(gs_info_.id());
    InetAddress node_addr(gs_info_.ip(), gs_info_.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, node_addr);
    server_->subscribe<OnBeConnectedEvent>(*this);
    server_->registerService(&gs_service_impl_);
    for (auto& it : g_server_service)
    {
        server_->registerService(it.get());
    }
    server_->start();   
}

void GameServer::OnAcquireLobbyInfoReplied(LobbyInfoResponse& replied)
{
    //connect controller
	auto& lobby_controllers = replied.lobby_controllers();
	for (int32_t i = 0; i < lobby_controllers.controllers_size(); ++i)
	{
		auto& controller_node_info = lobby_controllers.controllers(i);
		InetAddress controller_addr(controller_node_info.ip(), controller_node_info.port());
		auto it = game_tls.controller_node().emplace(controller_node_info.id(), std::make_shared<ControllerNode>());
		auto& controller_node = *it.first->second;
		controller_node.session_ = std::make_shared<ControllerSessionPtr::element_type>(loop_, controller_addr);
		controller_node.node_info_.set_node_id(controller_node_info.id());
		auto& controller_node_session = controller_node.session_;
		controller_node_session->registerService(&gs_service_impl_);
        for (auto& it : g_server_service)
        {
            controller_node_session->registerService(it.get());
        }
		controller_node_session->subscribe<OnConnected2ServerEvent>(*this);
		controller_node_session->connect();
	}
}

void GameServer::CallControllerStartGs(ControllerSessionPtr controller_node)
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
    controller_node->CallMethod(ControllerServiceStartGsMsgId,rq);
    LOG_DEBUG << "connect to controller" ;
}

void GameServer::CallLobbyStartGs()
{
    auto server_type = tls.registry.get<GsServerType>(global_entity()).server_type_;
    if (!(server_type == kMainSceneCrossServer ||
        server_type == kRoomSceneCrossServer))
    {
        return;
    }
    StartCrossGsRequest rq;
	auto session_info = rq.mutable_rpc_client();
	auto node_info = rq.mutable_rpc_server();
	session_info->set_ip(lobby_node_->local_addr().toIp());
	session_info->set_port(lobby_node_->local_addr().port());
	node_info->set_ip(gs_info_.ip());
	node_info->set_port(gs_info_.port());
    rq.set_server_type(server_type);
	rq.set_gs_node_id(gs_info_.id());
    lobby_node_->CallMethod(LobbyServiceStartCrossGsMsgId, rq);
}

void GameServer::receive(const OnConnected2ServerEvent& es)
{
    auto& conn = es.conn_;
    if (deploy_node_->peer_addr().toIpPort() == conn->peerAddress().toIpPort())
    {
        // started 
        if (nullptr != server_)
        {
            return;
        }

        EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
            [this]() ->void
            {
                ServerInfoRequest rq;
                rq.set_group(GameConfig::GetSingleton().config_info().group_id());
                rq.set_lobby_id(LobbyConfig::GetSingleton().config_info().lobby_id());
                deploy_node_->CallMethod(DeployServiceServerInfoMsgId, rq);
            }
        );
    }

    for (auto& it : game_tls.controller_node())
    {
        auto& controller_node = it.second;
        auto& controller_session = controller_node->session_;
        if (conn->connected() &&
            IsSameAddr(controller_session->peer_addr(), conn->peerAddress()))
        {
            EventLoop::getEventLoopOfCurrentThread()->queueInLoop(std::bind(&GameServer::CallControllerStartGs, this, controller_session));
            break;
        }
        // ms 走断线重连，不删除
    }

    if (nullptr != lobby_node_)
    {
		if (conn->connected() && 
            IsSameAddr(lobby_node_->peer_addr(), conn->peerAddress()))
		{
			EventLoop::getEventLoopOfCurrentThread()->queueInLoop(std::bind(&GameServer::CallLobbyStartGs, this));
		}
		else if (!conn->connected() &&
			IsSameAddr(lobby_node_->peer_addr(), conn->peerAddress()))
		{

		}
    }
  
}

void GameServer::receive(const OnBeConnectedEvent& es)
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

void GameServer::Connect2Lobby()
{
    lobby_node_->registerService(&gs_service_impl_);
    lobby_node_->subscribe<OnConnected2ServerEvent>(*this);
    lobby_node_->connect();
}
