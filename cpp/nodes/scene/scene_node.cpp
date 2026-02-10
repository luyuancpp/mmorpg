#include "scene_node.h"

#include <ranges>

#include "table/code/all_table.h"
#include "core/config/config.h"
#include "handler/event/event_handler.h"
#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "network/rpc_session.h"
#include "proto/common/base/node.pb.h"
#include "proto/common/event/server_event.pb.h"
#include "threading/redis_manager.h"
#include "frame/manager/frame_time.h"
#include "world/world.h"
#include "proto/centre/centre_service.pb.h"
#include "core/network/message_system.h"
#include "kafka/system/kafka.h"
#include "core/system/redis.h"

using namespace muduo::net;

SceneNode::SceneNode(muduo::net::EventLoop* loop)
    :Node(loop, "logs/scene")
{
	GetNodeInfo().set_node_type(RoomNodeService);
	targetNodeTypeWhitelist = { CentreNodeService };

	tlsRedisSystem.Initialize();

	EventHandler::Register();


	World::InitializeSystemBeforeConnect();

	OnTablesLoadSuccess([]()
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


