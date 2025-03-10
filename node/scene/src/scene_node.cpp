#include "scene_node.h"

#include <ranges>

#include "all_config.h"
#include "core/config/config_system.h"
#include "game_config/deploy_json.h"
#include "grpc/generator/deploy_service_grpc.h"
#include "handler/event/event_handler.h"
#include "handler/service/register_handler.h"
#include "handler/service/player/player_service.h"
#include "handler/service_replied/player/player_service_replied.h"
#include "log/constants/log_constants.h"
#include "log/system/console_log_system.h"
#include "muduo/base/Logging.h"
#include "muduo/base/TimeZone.h"
#include "muduo/net/InetAddress.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "node/scene_node_info.h"
#include "proto/common/deploy_service.grpc.pb.h"
#include "proto/logic/constants/node.pb.h"
#include "proto/logic/event/server_event.pb.h"
#include "service_info/service_info.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "time/system/time_system.h"
#include "util/game_registry.h"
#include "world/world.h"

SceneNode* gSceneNode = nullptr;

using namespace muduo::net;

muduo::AsyncLogging& logger()
{
    return  gSceneNode->Log();
}

SceneNode::SceneNode(muduo::net::EventLoop* loop)
    :Node(loop, "logs/game"),
     redis(std::make_shared<PbSyncRedisClientPtr::element_type>())
{
}

NodeInfo& SceneNode::GetNodeInfo()
{
    return gSceneNodeInfo.GetNodeInfo();
}

void SceneNode::Initialize()
{
    gSceneNode = this;
    
    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&SceneNode::Receive1>(*this);
    tls.dispatcher.sink<OnBeConnectedEvent>().connect<&SceneNode::Receive2>(*this);
    
    Node::Initialize();
    EventHandler::Register();

    void InitGrpcDeployServiceResponseHandler();
    InitGrpcDeployServiceResponseHandler();

    InitPlayerService();
    
    void InitRepliedHandler();
    InitRepliedHandler();
    
    InitPlayerServiceReplied();
    
    World::InitializeSystemBeforeConnect();
}

void SceneNode::ShutdownNode(){
    Node::ShutdownNode();
    
    tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&SceneNode::Receive1>(*this);
    tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&SceneNode::Receive2>(*this);
}

void SceneNode::StartRpcServer(const ::nodes_info_data& info)
{
    nodesInfo = info;
    
    InetAddress redis_addr(info.redis_info().redis_info(0).ip(), info.redis_info().redis_info(0).port());
    tlsGame.redis.Initialize(redis_addr);

    auto& nodeInfo = gSceneNodeInfo.GetNodeInfo();

    nodeInfo.set_game_node_type(ZoneConfig::GetSingleton().ConfigInfo().server_type());
    nodeInfo.set_node_type(eNodeType::kSceneNode);
    nodeInfo.set_launch_time(TimeUtil::NowSecondsUTC());

    InetAddress service_addr(GetNodeConf().ip(), GetNodeConf().port());
    rpcServer = std::make_unique<RpcServerPtr::element_type>(loop_, service_addr);
    
    rpcServer->registerService(&gameService);
    for ( auto & val : g_server_service | std::views::values )
    {
        rpcServer->registerService(val.get());
    }

    rpcServer->start();

    Node::StartRpcServer(info);

    ConnectToCentreHelper(&gameService);
    
    ReadyForGame();
    
    worldTimer.RunEvery(tlsGame.frameTime.delta_time(), World::Update);
    LOG_INFO << "game node  start " << GetNodeConf().DebugString();
}

void SceneNode::PrepareForBeforeConnection()
{
    Node::PrepareForBeforeConnection();
}

void SceneNode::ReadyForGame()
{
    Node::ReadyForGame();
    World::ReadyForGame();
}

void SceneNode::Receive1(const OnConnected2ServerEvent& es)
{
    if ( auto& conn = es.conn_ ; conn->connected())
    {
        for (const auto& [it, centre_node] : tls.centreNodeRegistry.view<RpcClientPtr>().each())
        {
            if (conn->connected() &&
                IsSameAddress(centre_node->peer_addr(), conn->peerAddress()))
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

void SceneNode::Receive2(const OnBeConnectedEvent& es)
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
            if (session.connection->peerAddress().toIpPort() != current_addr.toIpPort())
            {
                continue;
            }
            
            for (const auto& [gate_e, gate_node] : tls.gateNodeRegistry.view<RpcSessionPtr>().each())
            {
                if (nullptr != gate_node &&
                    gate_node->connection->peerAddress().toIpPort() == current_addr.toIpPort())
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

void SceneNode::OnConfigLoadSuccessful()
{
    ConfigSystem::OnConfigLoadSuccessful();
}

const game_node_db& SceneNode::GetNodeConf() 
{
    return nodesInfo.game_info().game_info(GetNodeId());
}

uint32_t SceneNode::GetNodeType() const
{
    return kSceneNode;
}

