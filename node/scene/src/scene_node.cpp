#include "scene_node.h"

#include <ranges>

#include "core/config/config_system.h"
#include "handler/event/event_handler.h"
#include "handler/service/register_handler.h"
#include "handler/service/player/player_service.h"
#include "handler/service_replied/player/player_service_replied.h"
#include "log/constants/log_constants.h"
#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "network/rpc_session.h"
#include "node/scene_node_info.h"
#include "proto/logic/constants/node.pb.h"
#include "proto/logic/event/server_event.pb.h"
#include "thread_local/storage.h"
#include "thread_local/storage_common_logic.h"
#include "thread_local/storage_game.h"
#include "time/system/time_system.h"
#include "util/game_registry.h"
#include "util/network_utils.h"
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
    
    tls.dispatcher.sink<OnConnected2TcpServerEvent>().connect<&SceneNode::Receive1>(*this);
    tls.dispatcher.sink<OnBeConnectedEvent>().connect<&SceneNode::Receive2>(*this);
    
    Node::Initialize();
    EventHandler::Register();

    InitPlayerService();
    
    InitPlayerServiceReplied();
    
    World::InitializeSystemBeforeConnect();
}

std::string SceneNode::GetServiceName() const
{
	return "sceneservice.rpc";
}

Node::ServiceList SceneNode::GetServiceList()
{
    ServiceList serviceList{};

    serviceList.emplace_back(&gameService);
	for (auto& val : gNodeService | std::views::values)
	{
        serviceList.emplace_back(val.get());
	}

    return serviceList;
}

void SceneNode::StartRpcServer()
{
    InetAddress redis_addr("127.0.0.1", 6379);
    tlsGame.redis.Initialize(redis_addr);

	Node::StartRpcServer();

    ConnectToCentreHelper(&gameService);
    
    ReadyForGame();
    
    worldTimer.RunEvery(tlsGame.frameTime.delta_time(), World::Update);
    LOG_INFO << "game node  start at " << GetNodeInfo().DebugString();
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

void SceneNode::Receive1(const OnConnected2TcpServerEvent& es)
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

uint32_t SceneNode::GetNodeType() const
{
    return kSceneNode;
}

