#include "game_node.h"

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/lobby_config.h"

#include "src/event_handler/event_handler.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/gate_session.h"
#include "src/network/rpc_session.h"
#include "src/network/process_info.h"
#include "service/centre_service_service.h"
#include "src/handler/player_service.h"
#include "src/replied_handler/player_service_replied.h"
#include "src/handler/register_handler.h"
#include "src/grpc/deploy/deployclient.h"
#include "src/thread_local/game_thread_local_storage.h"
#include "service/service.h"
#include "src/system/logic/config_system.h"
#include "src/network/node_info.h"
#include "src/util/game_registry.h"

#include "common_proto/deploy_service.grpc.pb.h"

GameNode* g_game_node = nullptr;

using namespace muduo::net;

void AsyncCompleteGrpc();

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

void GameNode::StartServer(const ::servers_info_data& info)
{
    node_net_info_ = info;
    InetAddress serverAddr(info.redis_info().ip(), info.redis_info().port());
    game_tls.redis_system().Init(serverAddr);

    node_info_.set_node_id(game_node_info().id());
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
    LOG_INFO << "game node  start " << game_node_info().DebugString();

    Connect2Centre();
}

void GameNode::RegisterGameToCentre(RpcClientPtr& centre_node)
{
    auto& centre_local_addr = centre_node->local_addr();
    RegisterGameRequest rq;
    rq.mutable_rpc_client()->set_ip(centre_local_addr.toIp());
    rq.mutable_rpc_client()->set_port(centre_local_addr.port());
    rq.mutable_rpc_server()->set_ip(game_node_info().ip());
    rq.mutable_rpc_server()->set_port(game_node_info().port());

    rq.set_server_type(tls.registry.get<GsNodeType>(global_entity()).server_type_);
    rq.set_game_node_id(game_node_id());
    centre_node->CallMethod(CentreServiceRegisterGameMsgId,rq);
    LOG_DEBUG << "connect to centre" ;
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
                EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
                    std::bind(&GameNode::RegisterGameToCentre, this, centre_node));
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

void GameNode::InitNodeByReqInfo()
{
    auto& zone = ZoneConfig::GetSingleton().config_info();

    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    auto deploy_channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(deploy_channel);
    g_deploy_client = std::make_unique_for_overwrite<DeployClient>();
    EventLoop::getEventLoopOfCurrentThread()->runEvery(0.01, AsyncCompleteGrpc);
    {
        NodeInfoRequest req;
        req.set_zone_id(ZoneConfig::GetSingleton().config_info().zone_id());
        void SendGetNodeInfo(NodeInfoRequest & request);
        SendGetNodeInfo(req);
    }
}

void GameNode::Connect2Centre()
{
    auto& centre_node_info = node_net_info_.centre_info();
    auto eid = entt::entity{ centre_node_info.id() };
    auto centre_node_id = tls.centre_node_registry.create(eid);
    if (centre_node_id != eid)
    {
        LOG_ERROR << "create centre error ";
    }
    InetAddress centre_addr(centre_node_info.ip(), centre_node_info.port());
    auto& centre_node = tls.centre_node_registry.emplace<RpcClientPtr>(
        centre_node_id,
        std::make_unique<RpcClientPtr::element_type>(loop_, centre_addr));
    centre_node->registerService(&game_service_);
    centre_node->connect();
}
