#include "centre_node.h"

#include <grpcpp/grpcpp.h>

#include "muduo//net/EventLoop.h"

#include "all_config.h"
#include "game_config/deploy_json.h"
#include "game_config/lobby_config.h"
#include "event_handler/event_handler.h"

#include "handler/player_service.h"
#include "replied_handler/player_service_replied.h"
#include "handler/register_handler.h"
#include "service/service.h"
#include "service/gate_service_service.h"
#include "thread_local/centre_thread_local_storage.h"
#include "grpc/deploy/deployclient.h"

#include "common_proto/deploy_service.grpc.pb.h"
#include "constants_proto/node.pb.h"

using namespace muduo;
using namespace net;

CentreNode* g_centre_node = nullptr;

void set_server_sequence_node_id(uint32_t node_id);
void InitRepliedHandler();
void AsyncCompleteGrpc();

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
    muduo::Logger::setLogLevel((muduo::Logger::LogLevel)ZoneConfig::GetSingleton().config_info().loglevel());


	InitNodeByReqInfo();

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
    auto& zone = ZoneConfig::GetSingleton().config_info();

    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    auto deploy_channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(deploy_channel);
    g_deploy_cq = std::make_unique_for_overwrite<CompletionQueue>();
    EventLoop::getEventLoopOfCurrentThread()->runEvery(0.01, AsyncCompleteGrpc);

    {
        NodeInfoRequest req;
        req.set_zone_id(ZoneConfig::GetSingleton().config_info().zone_id());
        void SendGetNodeInfo(NodeInfoRequest & request);
        SendGetNodeInfo(req);
    }
}

void CentreNode::StartServer(const ::nodes_info_data& info)
{
    serverinfos_ = info;
    auto& my_node_info = serverinfos_.centre_info().centre_info()[centre_node_index()];
    InetAddress servcie_addr(my_node_info.ip(), my_node_info.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, servcie_addr);
    tls.dispatcher.sink<OnBeConnectedEvent>().connect<&CentreNode::Receive2>(*this);
    server_->registerService(&contoller_service_);
    for (auto& it : g_server_service)
    {
        server_->registerService(it.second.get());
    }
    server_->start();
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
    auto& conn = es.conn_;
    if (conn->connected())
    {
		auto e = tls.network_registry.create();
		tls.network_registry.emplace<RpcSession>(e, RpcSession{ conn });
    }
    else
    {
		auto& current_addr = conn->peerAddress();
        for (auto game_e : tls.game_node_registry.view<RpcSessionPtr>())
        {
            auto game_node = tls.game_node_registry.try_get<RpcSessionPtr>(game_e);//如果是游戏逻辑服则删除
            if (nullptr != game_node && 
                (*game_node)->conn_->peerAddress().toIpPort() == current_addr.toIpPort())
            {
                Destroy(tls.game_node_registry, game_e);
                break;
            }
        }
            
        for (auto gate_e : tls.gate_node_registry.view<RpcSessionPtr>())
        {
            auto gate_node = tls.gate_node_registry.try_get<RpcSessionPtr>(gate_e);//如果是游戏逻辑服则删除
            if (nullptr != gate_node &&
                (*gate_node)->conn_->peerAddress().toIpPort() == current_addr.toIpPort())
            {
                //remove AfterChangeGsEnterScene
                //todo 
                Destroy(tls.gate_node_registry, gate_e);
                break;
            }
        }

        for (auto e : tls.network_registry.view<RpcSession>())
        {
            auto& sesion_addr =
                tls.network_registry.get<RpcSession>(e).conn_->peerAddress();
            if (sesion_addr.toIpPort() != current_addr.toIpPort())
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
    std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    auto deploy_channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(deploy_channel);
    g_deploy_cq = std::make_unique_for_overwrite<CompletionQueue>();

    void AsyncCompleteGrpc();
    EventLoop::getEventLoopOfCurrentThread()->runEvery(0.01, AsyncCompleteGrpc);

    NodeInfoRequest req;
    req.set_zone_id(zone.zone_id());
    void SendGetNodeInfo(NodeInfoRequest & req);
    SendGetNodeInfo(req);
}