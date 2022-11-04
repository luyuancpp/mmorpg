#include "game_server.h"

#include "muduo/base/Logging.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"

#include "src/game_config/all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/lobby_config.h"

#include "src/event_receiver/event_receiver.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/game_registry.h"
#include "src/network/gate_node.h"
#include "src/service/logic/player_service.h"
#include "src/service/logic/server_service.h"
#include "src/service/server_replied.h"

#include "src/network/node_info.h"
#include "src/pb/pbc/msgmap.h"
#include "src/system/redis_system.h"
#include "src/system/logic/config_system.h"

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
    deploy_session_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_session_->subscribe<RegisterStubEvent>(deploy_stub_);
    deploy_session_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_session_->connect();
}

void GameServer::ServerInfo(ServerInfoRpc replied)
{
    auto& resp = replied->s_rp_;
    auto& info = replied->s_rp_->info();
   
    auto& lobby_info = info.regin_info();
    InetAddress lobby_addr(lobby_info.ip(), lobby_info.port());
   
    lobby_session_ = std::make_unique<RpcClient>(loop_, lobby_addr);
    
    InetAddress serverAddr(info.redis_info().ip(), info.redis_info().port());
    g_redis_system.Init(serverAddr);

    StartGsRpc rpc(std::make_shared<StartGsRpc::element_type>());
    rpc->s_rq_.set_group(GameConfig::GetSingleton().config_info().group_id());
    rpc->s_rq_.mutable_my_info()->set_ip(muduo::ProcessInfo::localip());
    rpc->s_rq_.mutable_my_info()->set_id(gs_info_.id());
    rpc->s_rq_.mutable_rpc_client()->set_ip(deploy_session_->local_addr().toIp());
    rpc->s_rq_.mutable_rpc_client()->set_port(deploy_session_->local_addr().port());
    deploy_stub_.CallMethod(
        &GameServer::StartGsDeployReplied,
        rpc,
        this,
        &deploy::DeployService_Stub::StartGS);

	LobbyInfoRpc rcp(std::make_shared<LobbyInfoRpc::element_type>());
    rcp->s_rq_.set_lobby_id(LobbyConfig::GetSingleton().config_info().lobby_id());
	deploy_stub_.CallMethod(
		&GameServer::LobbyInfoReplied,
        rcp,
		this,
		&deploy::DeployService_Stub::RegionInfo);
}

void GameServer::StartGsDeployReplied(StartGsRpc replied)
{
    Connect2Lobby();

    auto& redisinfo = replied->s_rp_->redis_info();
	redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);

    gs_info_ = replied->s_rp_->my_info();
    InetAddress node_addr(gs_info_.ip(), gs_info_.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, node_addr);
    server_->subscribe<OnBeConnectedEvent>(*this);
    server_->registerService(&gs_service_impl_);
    for (auto& it : g_server_nomal_service)
    {
        server_->registerService(it.get());
    }
    server_->start();   
}

void GameServer::LobbyInfoReplied(LobbyInfoRpc replied)
{
    //connect controller
    auto& resp = replied->s_rp_;
	auto& lobby_controllers = resp->lobby_controllers();
	for (int32_t i = 0; i < lobby_controllers.controllers_size(); ++i)
	{
		auto& controller_node_info = lobby_controllers.controllers(i);
		InetAddress controller_addr(controller_node_info.ip(), controller_node_info.port());
		auto it = g_controller_nodes->emplace(controller_node_info.id(), std::make_shared<ControllerNode>());
		auto& controller_node = *it.first->second;
		controller_node.session_ = std::make_shared<ControllerSessionPtr::element_type>(loop_, controller_addr);
		controller_node.node_info_.set_node_id(controller_node_info.id());
		auto& controller_node_session = controller_node.session_;
        auto& controller_stub = registry.emplace<RpcStub<controllerservice::ControllerNodeService_Stub>>(controller_node.controller_);
        controller_node_session->subscribe<RegisterStubEvent>(controller_stub);
		controller_node_session->subscribe<RegisterStubEvent>(g2controller_stub_);
		controller_node_session->registerService(&gs_service_impl_);
        for (auto& it : g_server_nomal_service)
        {
            controller_node_session->registerService(it.get());
        }
		controller_node_session->subscribe<OnConnected2ServerEvent>(*this);
		controller_node_session->connect();
	}
}

void GameServer::CallControllerStartGs(ControllerSessionPtr& controller_node)
{
    ServerReplied::StartGsControllerRpc rpc(std::make_shared<ServerReplied::StartGsControllerRpc::element_type>());
    auto& controller_local_addr = controller_node->local_addr();
    controllerservice::StartGsRequest& request = rpc->s_rq_;
    auto session_info = request.mutable_rpc_client();
    auto node_info = request.mutable_rpc_server();
    session_info->set_ip(controller_local_addr.toIp());
    session_info->set_port(controller_local_addr.port());
    node_info->set_ip(gs_info_.ip());
    node_info->set_port(gs_info_.port());
    request.set_server_type(registry.get<GsServerType>(global_entity()).server_type_);
    request.set_gs_node_id(gs_info_.id());
    g2controller_stub_.CallMethod(
        &ServerReplied::StartGsControllerReplied,
        rpc,
        &ServerReplied::GetSingleton(),
        &controllerservice::ControllerNodeService_Stub::StartGs);
    LOG_DEBUG << "conncet to controller" ;
}

void GameServer::Register2Lobby()
{
    auto server_type = registry.get<GsServerType>(global_entity()).server_type_;
    if (!(server_type == kMainSceneCrossServer ||
        server_type == kRoomSceneCrossServer))
    {
        return;
    }
	ServerReplied::StartCrossGsRpc rpc(std::make_shared< ServerReplied::StartCrossGsRpc::element_type>());
	auto& rq = rpc->s_rq_;
	auto session_info = rq.mutable_rpc_client();
	auto node_info = rq.mutable_rpc_server();
	session_info->set_ip(lobby_session_->local_addr().toIp());
	session_info->set_port(lobby_session_->local_addr().port());
	node_info->set_ip(gs_info_.ip());
	node_info->set_port(gs_info_.port());
    rq.set_server_type(server_type);
	rq.set_gs_node_id(gs_info_.id());
	lobby_stub_.CallMethod(
		&ServerReplied::StartCrossGsReplied,
		rpc,
		&ServerReplied::GetSingleton(),
		&lobbyservcie::LobbyService_Stub::StartCrossGs);

}

void GameServer::receive(const OnConnected2ServerEvent& es)
{
    auto& conn = es.conn_;
    if (deploy_session_->peer_addr().toIpPort() == conn->peerAddress().toIpPort())
    {
        // started 
        if (nullptr != server_)
        {
            return;
        }

        EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
            [this]() ->void
            {
                ServerInfoRpc rpc(std::make_shared<ServerInfoRpc::element_type>());
                rpc->s_rq_.set_group(GameConfig::GetSingleton().config_info().group_id());
                rpc->s_rq_.set_lobby_id(LobbyConfig::GetSingleton().config_info().lobby_id());
                deploy_stub_.CallMethod(
                    &GameServer::ServerInfo,
                    rpc,
                    this,
                    &deploy::DeployService_Stub::ServerInfo);
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

    if (nullptr != lobby_session_)
    {
		if (conn->connected() && 
            IsSameAddr(lobby_session_->peer_addr(), conn->peerAddress()))
		{
			EventLoop::getEventLoopOfCurrentThread()->queueInLoop(std::bind(&GameServer::Register2Lobby, this));
		}
		else if (!conn->connected() &&
			IsSameAddr(lobby_session_->peer_addr(), conn->peerAddress()))
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
    lobby_session_->subscribe<RegisterStubEvent>(lobby_stub_);
    lobby_session_->registerService(&gs_service_impl_);
    lobby_session_->subscribe<OnConnected2ServerEvent>(*this);
    lobby_session_->connect();
}
