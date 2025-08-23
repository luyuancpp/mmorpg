#include "scene_node.h"

#include <ranges>

#include "all_config.h"
#include "core/config/config_system.h"
#include "handler/event/event_handler.h"
#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "network/rpc_session.h"
#include "proto/common/node.pb.h"
#include "proto/logic/event/server_event.pb.h"
#include "thread_local/redis_manager.h"
#include "frame/manager/frame_time_manager.h"
#include "world/world.h"
#include "proto/centre/centre_service.pb.h"
#include "core/network/message_system.h"
#include "kafka/system/kafka_system.h"
#include "core/system/redis_system.h"

using namespace muduo::net;

SceneNode::SceneNode(muduo::net::EventLoop* loop)
    :Node(loop, "logs/scene")
{
	GetNodeInfo().set_node_type(SceneNodeService);
	targetNodeTypeWhitelist = { CentreNodeService };

	tlsRedisSystem.Initialize();

	EventHandler::Register();


	World::InitializeSystemBeforeConnect();

	OnConfigLoadSuccess([]()
	{
		ConfigSystem::OnConfigLoadSuccessful();
	});

	GetKafkaManager().SetKafkaHandler(KafkaSystem::KafkaMessageHandler);
}

void SceneNode::StartRpcServer(){
	Node::StartRpcServer();

	World::ReadyForGame();
	worldTimer.RunEvery(tlsFrameTimeManager.frameTime.delta_time(), World::Update);
}


