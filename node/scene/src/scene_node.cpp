#include "scene_node.h"

#include <ranges>

#include "all_config.h"
#include "core/config/config_system.h"
#include "game_config/deploy_json.h"
#include "grpc/deploy/deploy_client.h"
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

void AsyncOutput(const char* msg, int len)
{
    gSceneNode->Log().append(msg, len);
#ifdef WIN32
    Log2Console(msg, len);
#endif
}

void InitRepliedHandler();

SceneNode::SceneNode(muduo::net::EventLoop* loop)
    :loop_(loop),
     muduoLog { "logs/game", kMaxLogFileRollSize, 1},
     redis(std::make_shared<PbSyncRedisClientPtr::element_type>())
{
}

SceneNode::~SceneNode()
{
    Exit();
}

const NodeInfo& SceneNode::GetNodeInfo() const
{
    return gSceneNodeInfo.GetNodeInfo();
}

void SceneNode::Init()
{
    gSceneNode = this; 
    EventHandler::Register();

    InitLog();
    InitNodeConfig();
    muduo::Logger::setLogLevel(static_cast <muduo::Logger::LogLevel> (
        ZoneConfig::GetSingleton().ConfigInfo().loglevel()));
    InitGameConfig();	
    
    InitMessageInfo();

    InitDeployServiceCompletedQueue(tls.grpc_node_registry, GlobalGrpcNodeEntity());

    void InitGrpcDeploySercieResponseHandler();
    InitGrpcDeploySercieResponseHandler();

    InitPlayerService();
    InitRepliedHandler();
    InitPlayerServiceReplied();
    void InitServiceHandler();
    InitServiceHandler();
    World::InitializeSystemBeforeConnect();

    InitNodeByReqInfo();
}


void SceneNode::InitLog ( )
{
    InitTimeZone();
    muduo::Logger::setOutput(AsyncOutput);
    muduoLog.start();
}

void SceneNode::Exit ( )
{
    muduoLog.stop();
    tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&SceneNode::Receive1>(*this);
    tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&SceneNode::Receive2>(*this);

    ReleaseNodeId();
}

void  SceneNode::InitNodeConfig ( )
{
    ZoneConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
}

void SceneNode::InitGameConfig()
{
    LoadAllConfig();
    LoadAllConfigAsyncWhenServerLaunch();
    ConfigSystem::OnConfigLoadSuccessful();
}

void SceneNode::InitTimeZone()
{
    const muduo::TimeZone tz("zoneinfo/Asia/Hong_Kong");
    muduo::Logger::setTimeZone(tz);
}

void SceneNode::ReleaseNodeId() const
{
    ReleaseIDRequest request;
    request.set_id(GetNodeId());
    request.set_node_type(kSceneNode);
    DeployServiceReleaseID( tls.grpc_node_registry.get<GrpcDeployServiceStubPtr>(GlobalGrpcNodeEntity()), request);
}

void SceneNode::StartServer(const ::nodes_info_data& info)
{
    nodeServiceInfo = info;
    InetAddress redis_addr(info.redis_info().redis_info(0).ip(), info.redis_info().redis_info(0).port());
    tlsGame.redis.Initialize(redis_addr);

    auto& nodeInfo = gSceneNodeInfo.GetNodeInfo();

    nodeInfo.set_game_node_type(ZoneConfig::GetSingleton().ConfigInfo().server_type());
    nodeInfo.set_node_type(eNodeType::kSceneNode);
    nodeInfo.set_launch_time(TimeUtil::NowSecondsUTC());

    InetAddress service_addr(GetNodeConf().ip(), GetNodeConf().port());
    rpcServer = std::make_shared<RpcServerPtr::element_type>(loop_, service_addr);
    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&SceneNode::Receive1>(*this);
    tls.dispatcher.sink<OnBeConnectedEvent>().connect<&SceneNode::Receive2>(*this);

    rpcServer->registerService(&gameService);
    for ( auto & val : g_server_service | std::views::values )
    {
        rpcServer->registerService(val.get());
    }

    rpcServer->start();

    Connect2Centre();
    World::InitSystemAfterConnect();

    deployRpcTimer.Cancel();

    tls.dispatcher.trigger<OnServerStart>();
    
    worldTimer.RunEvery(tlsGame.frameTime.delta_time(), World::Update);
    LOG_INFO << "game node  start " << GetNodeConf().DebugString();
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

const game_node_db& SceneNode::GetNodeConf() const
{
    return nodeServiceInfo.game_info().game_info(GetNodeId());
}

void SceneNode::InitNodeByReqInfo()
{
    auto& zone = ZoneConfig::GetSingleton().ConfigInfo();
    const auto& deploy_info = DeployConfig::GetSingleton().DeployInfo();
    const std::string targetStr = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    
    auto& deployStub =
       tls.grpc_node_registry.emplace<GrpcDeployServiceStubPtr>(GlobalGrpcNodeEntity()) 
       = DeployService::NewStub(grpc::CreateChannel(targetStr, grpc::InsecureChannelCredentials()));

    deployRpcTimer.RunEvery(0.001, []() {
        HandleDeployServiceCompletedQueueMessage(tls.grpc_node_registry);
        }
    );

    {
        NodeInfoRequest request;
        request.set_node_type(kSceneNode);
        request.set_zone_id(ZoneConfig::GetSingleton().ConfigInfo().zone_id());
        DeployServiceGetNodeInfo(deployStub, request);
    }

    renewNodeLeaseTimer.RunEvery(kRenewLeaseTime, []() {
        auto& renewDeployStub =
        tls.grpc_node_registry.get<GrpcDeployServiceStubPtr>(GlobalGrpcNodeEntity());
        RenewLeaseIDRequest request;
        request.set_lease_id(gSceneNodeInfo.GetNodeInfo().lease_id());
        DeployServiceRenewLease(renewDeployStub, request);
        });
}

void SceneNode::Connect2Centre()
{
    for (auto& centre_node_info : nodeServiceInfo.centre_info().centre_info())
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
        centre_node->registerService(&gameService);
        centre_node->connect();
        if (centre_node_info.zone_id() ==
            ZoneConfig::GetSingleton().ConfigInfo().zone_id())
        {
            myZoneCentreNode = centre_node;
        }
    }
}
