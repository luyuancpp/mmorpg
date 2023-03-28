#include "controller_server.h"

#include "muduo/base/Logging.h"
#include "muduo//net/EventLoop.h"

#include "src/game_config/all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/lobby_config.h"

#include "src/event_receiver/event_receiver.h"
#include "src/network/gate_node.h"
#include "src/network/gs_node.h"
#include "src/network/rpc_client.h"
#include "src/util/game_registry.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/service/common_proto_replied/replied_dispathcer.h"
#include "src/service/logic_proto/player_service.h"
#include "src/service/logic_proto_replied/player_service_replied.h"
#include "src/service/logic_proto/server_service.h"
#include "src/pb/pbc/serviceid/service_method_id.h"
#include "src/pb/pbc/service_method/deploy_servicemethod.h"
#include "src/pb/pbc/service_method/gate_servicemethod.h"
#include "src/pb/pbc/service_method/lobby_scenemethod.h"
#include "src/thread_local/controller_thread_local_storage.h"

#include "game_service.pb.h"
#include "gate_service.pb.h"
#include "component_proto/player_network_comp.pb.h"

using namespace muduo;
using namespace net;

ControllerServer* g_controller_node = nullptr;

void set_server_squence_node_id(uint32_t node_id);
void InitFakeProtoServiceList();

uint32_t controller_node_id()
{
	return g_controller_node->controller_node_id();
}

ControllerServer::ControllerServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<PbSyncRedisClientPtr::element_type>())
{ 
    global_entity() = tls.registry.create();
}    

void ControllerServer::Init()
{
    g_controller_node = this;
    EventReceiverEvent::Register(tls.dispatcher);
    InitConfig();
	node_info_.set_node_type(kControllerNode);
	node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());
    muduo::Logger::setLogLevel((muduo::Logger::LogLevel)GameConfig::GetSingleton().config_info().loglevel());
    InitMsgService();
    InitPlayerServcie();
    InitPlayerServcieReplied();
    InitRepliedCallback();
    InitFakeProtoServiceList();
    //connect 
    Connect2Deploy();
}

void ControllerServer::Connect2Deploy()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_session_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_session_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_session_->connect();
}

void ControllerServer::StartServer(const ::servers_info_data& info)
{
    serverinfos_ = info;
    auto& databaseinfo = serverinfos_.database_info();
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_session_ = std::make_unique<RpcClient>(loop_, database_addr);
    db_session_->connect();    

    Connect2Lobby();
	
    auto& myinfo = serverinfos_.controller_info();
    node_info_.set_node_id(myinfo.id());
    InetAddress controller_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, controller_addr);
    server_->subscribe<OnBeConnectedEvent>(*this);
    server_->registerService(&contoller_service_);
    for (auto& it : g_server_service)
    {
        server_->registerService(it.get());
    }
    server_->start();
    LOG_INFO << "controller start " << myinfo.DebugString();
}


void ControllerServer::LetGateConnect2Gs(entt::entity gs, entt::entity gate)
{
    auto& connection_info = tls.registry.get<InetAddress>(gs);
    GateNodeStartGSRequest request;
    request.set_ip(connection_info.toIp());
    request.set_port(connection_info.port());
    request.set_gs_node_id(tls.registry.get<GsNodePtr>(gs)->node_id());
	tls.registry.get<GateNodePtr>(gate)->session_.Send(GateServiceStartGS, request);
}

void ControllerServer::receive(const OnConnected2ServerEvent& es)
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
                deploy_session_->CallMethod(DeployServiceServerInfo, &rq);
			}
			
            {
                SceneSqueueRequest rq;
                deploy_session_->CallMethod(DeployServiceSceneSqueueNodeId, &rq);
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

void ControllerServer::receive(const OnBeConnectedEvent& es)
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
                //remove AfterChangeGsEnterScene
				//todo 
                controller_tls.game_node().erase((*gsnode)->node_info_.node_id());
            }
			auto gatenode = tls.registry.try_get<GateNodePtr>(e);//如果是gate
			if (nullptr != gatenode && (*gatenode)->node_info_.node_type() == kGateNode)
			{
				//todo
                controller_tls.gate_nodes().erase((*gatenode)->node_info_.node_id());
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
	lobby_session_->subscribe<OnConnected2ServerEvent>(*this);
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
    lobby_session_->CallMethod(LobbyServiceStartControllerNode, &rq);
}