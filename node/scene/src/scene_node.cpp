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
#include "proto/common/centre_service.pb.h"
#include "core/network/message_system.h"
#include "service_info/centre_service_service_info.h"

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

void SceneNode::Initialize()
{
    GetNodeInfo().set_node_type(SceneNodeService);

    gSceneNode = this;
    
    
    Node::Initialize();

    EventHandler::Register();

    InitPlayerService();
    
    InitPlayerServiceReplied();
    
    World::InitializeSystemBeforeConnect();
}

void SceneNode::StartRpcServer()
{
    InetAddress redis_addr("127.0.0.1", 6379);
    tlsGame.redis.Initialize(redis_addr);

    rpcServer->registerService(&nodeReplyService);
	for (auto& val : gNodeService | std::views::values)
	{
        rpcServer->registerService(val.get());
	}

	Node::StartRpcServer();

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

void SceneNode::OnConfigLoadSuccessful()
{
    ConfigSystem::OnConfigLoadSuccessful();
}

Node::CanConnectNodeTypeList SceneNode::GetAllowedTargetNodeTypes()
{
	return { CentreNodeService };
}

