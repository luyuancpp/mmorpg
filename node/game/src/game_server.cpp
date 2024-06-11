#include "game_server.h"

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

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

#include "src/network/node_info.h"
#include "src/util/game_registry.h"

GameNode* g_game_node = nullptr;

using namespace muduo::net;

NodeId get_gate_node_id()
{
    return g_game_node->gs_info().id();
}

NodeId  game_node_id()
{
    return g_game_node->node_info().node_id();
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
    tls.registry.emplace<GsNodeType>(global_entity(), 
        GsNodeType{ ZoneConfig::GetSingleton().config_info().server_type() });
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
    LoadAllConfig();
	ZoneConfig::GetSingleton().Load("game.json");
	DeployConfig::GetSingleton().Load("deploy.json");
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

void GameNode::RegisterGameToCentre(RpcClientPtr& centre_node)
{
    auto& centre_local_addr = centre_node->local_addr();
    RegisterGameRequest rq;
    rq.mutable_rpc_client()->set_ip(centre_local_addr.toIp());
    rq.mutable_rpc_client()->set_port(centre_local_addr.port());
    rq.mutable_rpc_server()->set_ip(game_info_.ip());
    rq.mutable_rpc_server()->set_port(game_info_.port());

    rq.set_server_type(tls.registry.get<GsNodeType>(global_entity()).server_type_);
    rq.set_game_node_id(game_node_id());
    centre_node->CallMethod(CentreServiceRegisterGameMsgId,rq);
    LOG_DEBUG << "connect to centre" ;
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

    for (auto& it : tls.centre_node_registry.view<RpcClientPtr>())
    {
        auto& centre_node = tls.centre_node_registry.get<RpcClientPtr>(it);
        if (conn->connected() &&
            IsSameAddr(centre_node->peer_addr(), conn->peerAddress()))
        {
            EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
                std::bind(&GameNode::RegisterGameToCentre, this, centre_node));
            break;
        }
        // centre 走断线重连，不删除
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
		tls.registry.emplace<RpcSession>(e, RpcSession{ conn });
	}
    else
    {
        auto& current_addr = conn->peerAddress();
        for (auto e : tls.network_registry.view<RpcSession>())
        {
            auto& sesion_addr =
                tls.network_registry.get<RpcSession>(e).conn_->peerAddress();
            if (sesion_addr.toIpPort() != current_addr.toIpPort())
            {
                continue;
            }
            
            for (auto gate_e : tls.gate_node_registry.view<RpcSessionPtr>())
            {
                auto gate_node = tls.gate_node_registry.try_get<RpcSessionPtr>(gate_e);
                if (nullptr != gate_node &&
                    (*gate_node)->conn_->peerAddress().toIpPort() == current_addr.toIpPort())
                {
                    Destroy(tls.gate_node_registry, gate_e);
                    break;
                }
            }
            Destroy(tls.network_registry, e);
            break;
        }
    }
}

void GameNode::Connect2Lobby()
{
    lobby_node_->registerService(&gs_service_impl_);
    lobby_node_->connect();
}
