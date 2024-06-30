#include "centre_node.h"

#include <ranges>
#include <game_config/deploy_json.h>
#include <grpcpp/grpcpp.h>

#include "all_config.h"
#include "common_proto/deploy_service.grpc.pb.h"
#include "constants_proto/node.pb.h"
#include "event_handler/event_handler.h"
#include "game_config/lobby_config.h"
#include "grpc/deploy/deployclient.h"
#include "handler/player_service.h"
#include "handler/register_handler.h"
#include "muduo//net/EventLoop.h"
#include "network/rpc_session.h"
#include "replied_handler/player_service_replied.h"
#include "service/gate_service_service.h"
#include "service/service.h"
#include "system/player_session_system.h"
#include "thread_local/thread_local_storage_centre.h"

using namespace muduo;
using namespace net;

CentreNode* g_centre_node = nullptr;

void InitRepliedHandler();
void AsyncCompleteGrpcDeployService();

CentreNode::CentreNode(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<PbSyncRedisClientPtr::element_type>())
{ 
}

CentreNode::~CentreNode()
{
	tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&CentreNode::Receive2>(*this);
}

void CentreNode::Init()
{
    g_centre_node = this;
    EventHandler::Register();
    InitConfig();
	node_info_.set_node_type(kCentreNode);
	node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());
    muduo::Logger::setLogLevel(static_cast < muduo::Logger::LogLevel > (
        ZoneConfig::GetSingleton ( ) . config_info ( ) . loglevel ( ) ));

	InitNodeByReqInfo();
    InitSystemBeforeConnect();

    InitNodeServer();

    InitPlayerService();
    InitPlayerServiceReplied();
    InitRepliedHandler();
    InitMessageInfo();
   
    void InitServiceHandler();
    InitServiceHandler();
}

void CentreNode::InitNodeByReqInfo()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    const std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
    g_deploy_cq = std::make_unique_for_overwrite<CompletionQueue>();

    deploy_rpc_timer_.RunEvery(0.001, AsyncCompleteGrpcDeployService);

    {
        NodeInfoRequest req;
        req.set_zone_id(ZoneConfig::GetSingleton().config_info().zone_id());
        void SendGetNodeInfo(const NodeInfoRequest & request);
        SendGetNodeInfo(req);
    }
}

void CentreNode::StartServer(const ::nodes_info_data& info)
{
    server_infos_ = info;
    auto& my_node_info = server_infos_.centre_info().centre_info()[centre_node_index()];
   
    InetAddress service_addr(my_node_info.ip(), my_node_info.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, service_addr);
    tls.dispatcher.sink<OnBeConnectedEvent>().connect<&CentreNode::Receive2>(*this);
    server_->registerService(&centre_service_);
    for ( auto & val : g_server_service | std::views::values )
    {
        server_->registerService(val.get());
    }
    server_->start();
    deploy_rpc_timer_.Cancel();
    InitSystemAfterConnect();
    LOG_INFO << "centre start " << my_node_info.DebugString();
}


void CentreNode::BroadCastRegisterGameToGate(entt::entity game_node_id, entt::entity gate)
{
	auto gate_node = tls.gate_node_registry.try_get<RpcSessionPtr>(gate);
	if (nullptr == gate_node)
	{
		LOG_ERROR << "gate not found ";
		return;
	}
    auto game_node_service_addr = tls.game_node_registry.try_get<InetAddress>(game_node_id);
    if (nullptr == game_node_service_addr)
    {
        LOG_ERROR << "game not found ";
        return;
    }
    RegisterGameRequest request;
    request.mutable_rpc_server()->set_ip(game_node_service_addr->toIp());
    request.mutable_rpc_server()->set_port(game_node_service_addr->port());
    request.set_game_node_id(entt::to_integral(game_node_id));
    (*gate_node)->Send(GateServiceRegisterGameMsgId, request);
}

void CentreNode::SetNodeId(NodeId node_id)
{
    node_info_.set_node_id(node_id);
}

void CentreNode::Receive2(const OnBeConnectedEvent& es)
{
    if ( auto& conn = es.conn_ ; conn->connected())
    {
        const auto e = tls.network_registry.create();
		tls.network_registry.emplace<RpcSession>(e, RpcSession{ conn });
    }
    else
    {
		auto& current_addr = conn->peerAddress();
        for (const auto& [e, game_node]: tls.game_node_registry.view<RpcSessionPtr>().each())
        {
            //如果是游戏逻辑服则删除
            if (game_node->conn_->peerAddress().toIpPort() == current_addr.toIpPort())
            {
                Destroy(tls.game_node_registry, e);
                break;
            }
        }
            
        for (const auto& [e, gate_node] : tls.gate_node_registry.view<RpcSessionPtr>().each())
        {
            //如果是游戏逻辑服则删除
            if (nullptr != gate_node &&
                gate_node->conn_->peerAddress().toIpPort() == current_addr.toIpPort())
            {
                //remove AfterChangeGsEnterScene
                //todo 
                Destroy(tls.gate_node_registry, e);
                break;
            }
        }

        for (const auto& [e, session] : tls.network_registry.view<RpcSession>().each())
        {
            if (session.conn_->peerAddress().toIpPort() != current_addr.toIpPort())
            {
                continue;
            }
            Destroy(tls.network_registry, e);
            break;
        }
    }
}

void CentreNode::InitConfig()
{
    ZoneConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
    LobbyConfig::GetSingleton().Load("lobby.json");
    LoadAllConfigAsyncWhenServerLaunch();
}

void CentreNode::InitNodeServer()
{
    auto& zone = ZoneConfig::GetSingleton().config_info();

    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    const std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
    g_deploy_cq = std::make_unique_for_overwrite<CompletionQueue>();

    void AsyncCompleteGrpcDeployService();
    EventLoop::getEventLoopOfCurrentThread()->runEvery(0.01, AsyncCompleteGrpcDeployService);

    NodeInfoRequest req;
    req.set_zone_id(zone.zone_id());
    void SendGetNodeInfo(const NodeInfoRequest& req);
    SendGetNodeInfo(req);
}

void CentreNode::InitSystemBeforeConnect()
{
    PlayerSessionSystem::Init();
}

void CentreNode::InitSystemAfterConnect() const
{
    InetAddress redis_addr(server_infos_.redis_info().redis_info(0).ip(), 
        server_infos_.redis_info().redis_info(0).port());
    tls_centre.redis_system().Init(redis_addr);
}

