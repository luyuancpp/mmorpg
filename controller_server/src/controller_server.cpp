#include "controller_server.h"

#include "muduo//net/EventLoop.h"

#include "src/game_config/all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/lobby_config.h"

#include "src/event_handler/event_handler.h"
#include "src/network/gate_node.h"
#include "src/network/game_node.h"

#include "src/handler/player_service.h"
#include "src/replied_handler/player_service_replied.h"
#include "src/handler/register_handler.h"
#include "src/pb/pbc/service.h"
#include "src/pb/pbc/deploy_service_service.h"
#include "src/pb/pbc/gate_service_service.h"
#include "src/pb/pbc/lobby_scene_service.h"
#include "src/thread_local/controller_thread_local_storage.h"


using namespace muduo;
using namespace net;

ControllerServer* g_controller_node = nullptr;

void set_server_sequence_node_id(uint32_t node_id);
void InitRepliedHandler();

NodeId controller_node_id()
{
	return g_controller_node->controller_node_id();
}

ControllerServer::ControllerServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<PbSyncRedisClientPtr::element_type>())
{ 
}

ControllerServer::~ControllerServer()
{
	tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&ControllerServer::Receive1>(*this);
	tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&ControllerServer::Receive2>(*this);
}

void ControllerServer::Init()
{
    g_controller_node = this;
    EventHandler::Register();
    InitConfig();
	node_info_.set_node_type(kControllerNode);
	node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());
    muduo::Logger::setLogLevel((muduo::Logger::LogLevel)GameConfig::GetSingleton().config_info().loglevel());

	Connect2Deploy();

    InitPlayerService();
    InitPlayerServiceReplied();
    InitRepliedHandler();
    InitMessageInfo();
   
    void InitServiceHandler();
    InitServiceHandler();

}

void ControllerServer::Connect2Deploy()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_session_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&ControllerServer::Receive1>(*this);
    deploy_session_->connect();
}

void ControllerServer::StartServer(const ::servers_info_data& info)
{
    serverinfos_ = info;
    auto& database_info = serverinfos_.database_info();
    InetAddress database_addr(database_info.ip(), database_info.port());
    db_session_ = std::make_unique<RpcClient>(loop_, database_addr);
    db_session_->connect();    

    Connect2Lobby();
	
    auto& my_node_info = serverinfos_.controller_info();
    node_info_.set_node_id(my_node_info.id());
    InetAddress controller_addr(my_node_info.ip(), my_node_info.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, controller_addr);
    tls.dispatcher.sink<OnBeConnectedEvent>().connect<&ControllerServer::Receive2>(*this);
    server_->registerService(&contoller_service_);
    for (auto& it : g_server_service)
    {
        server_->registerService(it.second.get());
    }
    server_->start();
    LOG_INFO << "controller start " << my_node_info.DebugString();
}


void ControllerServer::LetGateConnect2Gs(entt::entity gs, entt::entity gate)
{
    auto game_node_ptr = tls.registry.try_get<GameNodePtr>(gs);
    if (nullptr == game_node_ptr)
    {
        LOG_ERROR << "gs not found ";
        return;
    }
	auto gate_node_ptr = tls.registry.try_get<GateNodePtr>(gate);
	if (nullptr == gate_node_ptr)
	{
		LOG_ERROR << "gate not found ";
		return;
	}
    auto& game_node = *game_node_ptr;
    GateNodeStartGSRequest request;
    request.set_ip(game_node->node_inet_addr_.toIp());
    request.set_port(game_node->node_inet_addr_.port());
    request.set_game_node_id(game_node->node_id());
    (*gate_node_ptr)->session_.Send(GateServiceStartGSMsgId, request);
}

void ControllerServer::Receive1(const OnConnected2ServerEvent& es)
{
	auto& conn = es.conn_;
    if (conn->connected())
    {
		// started 
		if (nullptr == server_)
		{
			{
                ServerInfoRequest rq;
                rq.set_group(GameConfig::GetSingleton().config_info().group_id());
                rq.set_lobby_id(LobbyConfig::GetSingleton().config_info().lobby_id());
                deploy_session_->CallMethod(DeployServiceServerInfoMsgId, rq);
			}
			
            {
                SceneSqueueRequest rq;
                deploy_session_->CallMethod(DeployServiceSceneSequenceNodeIdMsgId, rq);
            }
		}
		
		if (nullptr != lobby_session_)
		{
			if (conn->connected() && IsSameAddr(lobby_session_->peer_addr(), conn->peerAddress()))
			{
				EventLoop::getEventLoopOfCurrentThread()->queueInLoop(std::bind(&ControllerServer::Register2Lobby, this));
			}
			else if(!conn->connected() && IsSameAddr(lobby_session_->peer_addr(), conn->peerAddress()))
			{

			}
		}
    }
	else
	{
		
	}
	
}

void ControllerServer::Receive2(const OnBeConnectedEvent& es)
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
            auto gsnode = tls.registry.try_get<GameNodePtr>(e);//如果是游戏逻辑服则删除
            if (nullptr != gsnode && (*gsnode)->node_info_.node_type() == kGameNode)
            {
                //remove AfterChangeGsEnterScene
				//todo 
                controller_tls.game_node().erase((*gsnode)->node_info_.node_id());
            }
			auto gatenode = tls.registry.try_get<GateNodePtr>(e);
			if (nullptr != gatenode && (*gatenode)->node_info_.node_type() == kGateNode)
			{
				//todo
                controller_tls.gate_nodes().erase((*gatenode)->node_info_.node_id());
			}
			auto login_node = tls.registry.try_get<LoginNode>(e);
			if (nullptr != login_node && (*login_node).node_info_.node_type() == kLoginNode)
			{
				//todo
				controller_tls.login_node().erase((*login_node).node_info_.node_id());
			}
			tls.registry.destroy(e);
			break;
		}
    }
}

void ControllerServer::InitConfig()
{
    GameConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
    LobbyConfig::GetSingleton().Load("lobby.json");
    LoadAllConfigAsyncWhenServerLaunch();
}

void ControllerServer::Connect2Lobby()
{
	auto& lobby_info = serverinfos_.lobby_info();
	InetAddress lobby_addr(lobby_info.ip(), lobby_info.port());
	lobby_session_ = std::make_unique<RpcClient>(loop_, lobby_addr);
	lobby_session_->registerService(&contoller_service_);
	lobby_session_->connect();
}

void ControllerServer::Register2Lobby()
{
    auto& myinfo = serverinfos_.controller_info();
    StartControllerRequest rq;
	auto session_info = rq.mutable_rpc_client();
	auto node_info = rq.mutable_rpc_server();
	session_info->set_ip(lobby_session_->local_addr().toIp());
	session_info->set_port(lobby_session_->local_addr().port());
	node_info->set_ip(myinfo.ip());
	node_info->set_port(myinfo.port());
	rq.set_controller_node_id(myinfo.id());
    lobby_session_->CallMethod(LobbyServiceStartControllerNodeMsgId, rq);
}