#include "game_node.h"

#include <ranges>

#include "all_config.h"
#include "proto/common/deploy_service.grpc.pb.h"
#include "constants/file.h"
#include "proto/logic/constants/node.pb.h"
#include "handler/event/event_handler.h"
#include "proto/logic/event/server_event.pb.h"
#include "game_config/deploy_json.h"
#include "game_config/lobby_config.h"
#include "grpc/deploy/deployclient.h"
#include "handler/service/player_service.h"
#include "handler/service/register_handler.h"
#include "muduo/base/Logging.h"
#include "muduo/base/TimeZone.h"
#include "muduo/net/InetAddress.h"
#include "network/gate_session.h"
#include "network/rpc_session.h"
#include "handler/service_replied/player_service_replied.h"
#include "service/service.h"
#include "system/config/config_system.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "util/color_console_log.h"
#include "util/game_registry.h"
#include "system/world.h"

GameNode* gGameNode = nullptr;

using namespace muduo::net;

void AsyncCompleteGrpcDeployService();

void AsyncOutput(const char* msg, int len)
{
    gGameNode->Log().append(msg, len);
#ifdef WIN32
    Log2Console(msg, len);
#endif
}

void InitRepliedHandler();

GameNode::GameNode(muduo::net::EventLoop* loop)
    :loop_(loop),
     muduo_log_ { "logs/game", kMaxLogFileRollSize, 1},
     redis_(std::make_shared<PbSyncRedisClientPtr::element_type>())
{
}

GameNode::~GameNode()
{
    Exit (  );
}

void GameNode::Init()
{
    gGameNode = this; 
    EventHandler::Register();

    InitLog();
    InitNodeConfig();
    muduo::Logger::setLogLevel(static_cast <muduo::Logger::LogLevel> (
        ZoneConfig::GetSingleton().config_info().loglevel()));
    InitGameConfig();	
    
    InitMessageInfo();
    InitPlayerService();
    InitRepliedHandler();
    InitPlayerServiceReplied();
    void InitServiceHandler();
    InitServiceHandler();
    World::InitializeSystemBeforeConnect();

    InitNodeByReqInfo();
}


void GameNode::InitLog ( )
{
    InitTimeZone();
    muduo::Logger::setOutput(AsyncOutput);
    muduo_log_.start();
}

void GameNode::Exit ( )
{
    muduo_log_.stop();
    tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&GameNode::Receive1>(*this);
    tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&GameNode::Receive2>(*this);
}

void  GameNode::InitNodeConfig ( )
{
    ZoneConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
}

void GameNode::InitGameConfig()
{
    LoadAllConfig();
    LoadAllConfigAsyncWhenServerLaunch();
    ConfigSystem::OnConfigLoadSuccessful();
}

void GameNode::InitTimeZone()
{
    const muduo::TimeZone tz("zoneinfo/Asia/Hong_Kong");
    muduo::Logger::setTimeZone(tz);
}

void GameNode::SetNodeId( const NodeId node_id)
{
    node_info_.set_node_id(node_id);
}

void GameNode::StartServer(const ::nodes_info_data& info)
{
    node_net_info_ = info;
    InetAddress redis_addr(info.redis_info().redis_info(0).ip(), info.redis_info().redis_info(0).port());
    tlsGame.redis.Init(redis_addr);

    node_info_.set_game_node_type(ZoneConfig::GetSingleton().config_info().server_type());
    node_info_.set_node_type(eNodeType::kGameNode);
    node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());
    InetAddress service_addr(GetNodeConf().ip(), GetNodeConf().port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, service_addr);
    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&GameNode::Receive1>(*this);
    tls.dispatcher.sink<OnBeConnectedEvent>().connect<&GameNode::Receive2>(*this);
    server_->registerService(&game_service_);
    for ( auto & val : g_server_service | std::views::values )
    {
        server_->registerService(val.get());
    }
    server_->start();

    Connect2Centre();
    World::InitSystemAfterConnect();

    deploy_rpc_timer_.Cancel();

    tls.dispatcher.trigger<OnServerStart>();

    
    world_timer_.RunEvery(tlsGame.frameTime.delta_time(), World::Update);
    LOG_INFO << "game node  start " << GetNodeConf().DebugString();
}

void GameNode::Receive1(const OnConnected2ServerEvent& es)
{
    if ( auto& conn = es.conn_ ; conn->connected())
    {
        for (const auto& [it, centre_node] : tls.centreNodeRegistry.view<RpcClientPtr>().each())
        {
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
    if ( auto& conn = es.conn_ ; conn->connected())
	{
		auto e = tls.registry.create();
		tls.networkRegistry.emplace<RpcSession>(e, RpcSession{ conn });
	}
    else
    {
        auto& current_addr = conn->peerAddress();
        for (const auto& [e, session] : tls.networkRegistry.view<RpcSession>().each())
        {
            if (session.conn_->peerAddress().toIpPort() != current_addr.toIpPort())
            {
                continue;
            }
            
            for (const auto& [gate_e, gate_node] : tls.gateNodeRegistry.view<RpcSessionPtr>().each())
            {
                if (nullptr != gate_node &&
                    gate_node->conn_->peerAddress().toIpPort() == current_addr.toIpPort())
                {
                    Destroy(tls.gateNodeRegistry, gate_e);
                    break;
                }
            }
            Destroy(tls.networkRegistry, e);
            break;
        }
    }
}

const game_node_db& GameNode::GetNodeConf() const
{
    return node_net_info_.game_info().game_info(GetNodeConfIndex());
}

void GameNode::InitNodeByReqInfo()
{
    auto& zone = ZoneConfig::GetSingleton().config_info();
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    const std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    extern std::unique_ptr<DeployService::Stub> gDeployStub;
    gDeployStub = DeployService::NewStub(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
    gDeployCq = std::make_unique_for_overwrite<CompletionQueue>();
    deploy_rpc_timer_.RunEvery(0.001, AsyncCompleteGrpcDeployService);
    {
        NodeInfoRequest rq;
        rq.set_node_type(kGameNode);
        rq.set_zone_id(ZoneConfig::GetSingleton().config_info().zone_id());
        void SendGetNodeInfo(const NodeInfoRequest& request);
        SendGetNodeInfo(rq);
    }
}

void GameNode::Connect2Centre()
{
    for (auto& centre_node_info : node_net_info_.centre_info().centre_info())
    {
        entt::entity id{ centre_node_info.id() };
        const auto   centre_node_id = tls.centreNodeRegistry.create(id);
        if (centre_node_id != id)
        {
            LOG_ERROR << "centre id ";
            continue;
        }
        InetAddress centre_addr(centre_node_info.ip(), centre_node_info.port());
        const auto& centre_node = tls.centreNodeRegistry.emplace<RpcClientPtr>(centre_node_id,
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
