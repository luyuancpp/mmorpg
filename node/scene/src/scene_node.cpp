#include "scene_node.h"

#include <ranges>

#include "all_config.h"
#include "core/config/config_system.h"
#include "handler/event/event_handler.h"
#include "service/player_service.h"
#include "service/player_service_replied.h"
#include "log/constants/log_constants.h"
#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "network/rpc_session.h"
#include "proto/logic/constants/node.pb.h"
#include "proto/logic/event/server_event.pb.h"
#include "thread_local/storage_common_logic.h"
#include "thread_local/storage_game.h"
#include "world/world.h"
#include "proto/common/centre_service.pb.h"
#include "core/network/message_system.h"

SceneNode* gSceneNode = nullptr;

using namespace muduo::net;

SceneNode::SceneNode(muduo::net::EventLoop* loop)
    :Node(loop, "logs/scene")
{
	gSceneNode = this;
	GetNodeInfo().set_node_type(SceneNodeService);
	targetNodeTypeWhitelist = { CentreNodeService };

	tlsGame.redis.Initialize();

	EventHandler::Register();

	World::InitializeSystemBeforeConnect();

	OnConfigLoadSuccess([]()
	{
		ConfigSystem::OnConfigLoadSuccessful();
	});
}

void SceneNode::StartRpcServer()
{
	Node::StartRpcServer();

	World::ReadyForGame();
	worldTimer.RunEvery(tlsGame.frameTime.delta_time(), World::Update);
	
    LOG_INFO << "game node  start at " << GetNodeInfo().DebugString();
}


