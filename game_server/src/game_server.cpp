#include "game_server.h"

#include "muduo/base/Logging.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"

#include "src/game_config/all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/lobby_config.h"

#include "src/event_receiver/event_receiver.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/thread_local/game_registry.h"
#include "src/game_logic/thread_local/game_registry.h"
#include "src/network/gate_node.h"
#include "src/network/rpc_connection_event.h"
#include "src/pb/pbc/service_method/controller_servicemethod.h"
#include "src/pb/pbc/service_method/deploy_servicemethod.h"
#include "src/pb/pbc/service_method/lobby_scenemethod.h"
#include "src/service/logic_proto/player_service.h"
#include "src/service/logic_proto_replied/player_service_replied.h"
#include "src/service/logic_proto/server_service.h"
#include "src/service/common_proto_replied/server_replied.h"
#include "src/service/common_proto_replied/replied_dispathcer.h"

#include "src/network/node_info.h"
#include "src/pb/pbc/msgmap.h"
#include "src/system/redis_system.h"
#include "src/system/logic/config_system.h"

NodeId node_id()
{
    return g_gs->gs_info().id();
}

GameServer* g_gs = nullptr;

GameServer::GameServer(muduo::net::EventLoop* loop)
    :loop_(loop),
     redis_(std::make_shared<PbSyncRedisClientPtr::element_type>()){}

void GameServer::Init()
{
    g_gs = this; 
    EventReceiver::Register(dispatcher);
    InitConfig();
    muduo::Logger::setLogLevel((muduo::Logger::LogLevel)GameConfig::GetSingleton().config_info().loglevel());
    global_entity() = registry.create();
    registry.emplace<GsServerType>(global_entity(), GsServerType{ GameConfig::GetSingleton().config_info().server_type() });
    LOG_INFO << "server type" << GameConfig::GetSingleton().config_info().server_type();
    InitMsgService();
    InitPlayerServcie();
    InitPlayerServcieReplied();
    InitRepliedCallback();
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
    g_redis_system.Init(serverAddr);

    {
        StartGSRequest rq;
        rq.set_group(GameConfig::GetSingleton().config_info().group_id());
        rq.mutable_my_info()->set_ip(muduo::ProcessInfo::localip());
        rq.mutable_my_info()->set_id(gs_info_.id());
        rq.mutable_rpc_client()->set_ip(deploy_node_->local_addr().toIp());
        rq.mutable_rpc_client()->set_port(deploy_node_->local_addr().port());
        deploy_node_->CallMethod(DeployServiceStartGSMethodDesc, &rq);
    }
   
    {
        LobbyServerRequest rq;
        rq.set_lobby_id(LobbyConfig::GetSingleton().config_info().lobby_id());
        deploy_node_->CallMethod(DeployServiceAcquireLobbyInfoMethodDesc, &rq);//获取大厅服下所有服务器信息
    }
	
}

void GameServer::StartGsDeployReplied(const StartGSResponse& replied)
{
    Connect2Lobby();

    auto& redisinfo = replied.redis_info();
	redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);

    gs_info_ = replied.my_info();
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
		auto it = g_controller_nodes->emplace(controller_node_info.id(), std::make_shared<ControllerNode>());
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
    ControllerNodeStartGsRequest request;
    auto session_info = request.mutable_rpc_client();
    auto node_info = request.mutable_rpc_server();
    session_info->set_ip(controller_local_addr.toIp());
    session_info->set_port(controller_local_addr.port());
    node_info->set_ip(gs_info_.ip());
    node_info->set_port(gs_info_.port());
    request.set_server_type(registry.get<GsServerType>(global_entity()).server_type_);
    request.set_gs_node_id(gs_info_.id());
    controller_node->CallMethod(ControllerServiceStartGsMethodDesc,&request);
    LOG_DEBUG << "conncet to controller" ;
}

void GameServer::CallLobbyStartGs()
{
    auto server_type = registry.get<GsServerType>(global_entity()).server_type_;
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
    lobby_node_->CallMethod(LobbyServiceStartCrossGsMethodDesc, &rq);
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
                deploy_node_->CallMethod(DeployServiceServerInfoMethodDesc, &rq);
            }
        );
    }

    for (auto& it : *g_controller_nodes)
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
			auto gatenode = registry.try_get<GateNodePtr>(e);//如果是gate
			if (nullptr != gatenode && (*gatenode)->node_info_.node_type() == kGateNode)
			{
                g_gate_nodes->erase((*gatenode)->node_info_.node_id());
			}
			registry.destroy(e);
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
