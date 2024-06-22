#include "game_node.h"

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "all_config.h"
#include "game_config/deploy_json.h"
#include "game_config/lobby_config.h"

#include "event_handler/event_handler.h"
#include "thread_local/thread_local_storage.h"
#include "network/gate_session.h"
#include "network/rpc_session.h"
#include "network/process_info.h"
#include "service/centre_service_service.h"
#include "handler/player_service.h"
#include "replied_handler/player_service_replied.h"
#include "handler/register_handler.h"
#include "grpc/deploy/deployclient.h"
#include "thread_local/game_thread_local_storage.h"
#include "service/service.h"
#include "system/config/config_system.h"

#include "util/game_registry.h"

#include "common_proto/deploy_service.grpc.pb.h"
#include "constants_proto/node.pb.h"
#include "event_proto/server_event.pb.h"

GameNode* g_game_node = nullptr;

using namespace muduo::net;

void AsyncCompleteGrpcDeployService();

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
	
    muduo::Logger::setLogLevel((muduo::Logger::LogLevel)ZoneConfig::GetSingleton().config_info().loglevel());
    global_entity();
    InitMessageInfo();
    InitPlayerService();
    InitPlayerServiceReplied();
    InitRepliedHandler();
    InitNodeByReqInfo();

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

void GameNode::SetNodeId(NodeId node_id)
{
    node_info_.set_node_id(node_id);
}

void GameNode::StartServer(const ::nodes_info_data& info)
{
    node_net_info_ = info;
    InetAddress redis_addr(info.redis_info().redis_info(0).ip(), info.redis_info().redis_info(0).port());
    game_tls.redis_system().Init(redis_addr);

    node_info_.set_game_node_type(ZoneConfig::GetSingleton().config_info().server_type());
    node_info_.set_node_type(eNodeType::kGameNode);
    node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());
    InetAddress servcie_addr(game_node_info().ip(), game_node_info().port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, servcie_addr);
    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&GameNode::Receive1>(*this);
    tls.dispatcher.sink<OnBeConnectedEvent>().connect<&GameNode::Receive2>(*this);
    server_->registerService(&game_service_);
    for (auto& it : g_server_service)
    {
        server_->registerService(it.second.get());
    }
    server_->start();

    Connect2Centre();

    deploy_rpc_timer_.Cancel();

    LOG_INFO << "game node  start " << game_node_info().DebugString();
}

void GameNode::Receive1(const OnConnected2ServerEvent& es)
{
    auto& conn = es.conn_;
    if (conn->connected())
    {
        for (auto& it : tls.centre_node_registry.view<RpcClientPtr>())
        {
            auto& centre_node = tls.centre_node_registry.get<RpcClientPtr>(it);
            if (conn->connected() &&
                IsSameAddr(centre_node->peer_addr(), conn->peerAddress()))
            {
               

                OnConnect2Centre connect2centre_event;
                connect2centre_event.set_entity(entt::to_integral(it));
                tls.dispatcher.trigger(connect2centre_event);
                break;
            }
            // centre 走断线重连，不删除
        }
    }
    else
    {

    }

}

void GameNode::Receive2(const OnBeConnectedEvent& es)
{
    auto& conn = es.conn_;
	if (conn->connected())
	{
		auto e = tls.registry.create();
		tls.network_registry.emplace<RpcSession>(e, RpcSession{ conn });
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

const game_node_db& GameNode::game_node_info() const
{
    return node_net_info_.game_info().game_info(game_node_index());
}

void GameNode::InitNodeByReqInfo()
{
    auto& zone = ZoneConfig::GetSingleton().config_info();

    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    auto deploy_channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(deploy_channel);
    g_deploy_cq = std::make_unique_for_overwrite<CompletionQueue>();
    deploy_rpc_timer_.RunEvery(0.001, AsyncCompleteGrpcDeployService);
    {
        NodeInfoRequest req;
        req.set_zone_id(ZoneConfig::GetSingleton().config_info().zone_id());
        void SendGetNodeInfo(NodeInfoRequest & request);
        SendGetNodeInfo(req);
    }
}

void GameNode::Connect2Centre()
{
    for (auto& centre_node_info : node_net_info_.centre_info().centre_info())
    {
        entt::entity id{ centre_node_info.id() };
        auto centre_node_id = tls.centre_node_registry.create(id);
        if (centre_node_id != id)
        {
            LOG_ERROR << "centre id ";
            continue;
        }
        InetAddress centre_addr(centre_node_info.ip(), centre_node_info.port());
        auto& centre_node = tls.centre_node_registry.emplace<RpcClientPtr>(centre_node_id,
            std::make_shared<RpcClientPtr::element_type>(loop_, centre_addr));
        centre_node->registerService(&game_service_);
        centre_node->connect();
        if (centre_node_info.zone_id() ==
            ZoneConfig::GetSingleton().config_info().zone_id())
        {
            zone_centre_node_ = centre_node;
        }
    }

}
