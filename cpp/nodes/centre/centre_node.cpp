#include "centre_node.h"

#include <ranges>
#include <grpcpp/grpcpp.h>

#include "handler/event/event_handler.h"
#include "muduo/net/EventLoop.h"
#include "network/rpc_session.h"
#include "proto/common/base/node.pb.h"
#include "kafka/system/kafka.h"
#include "redis/system/redis.h"

using namespace muduo;
using namespace net;

CentreNode::CentreNode(muduo::net::EventLoop* loop)
	: Node(loop, "logs/centre"){
	GetNodeInfo().set_node_type(CentreNodeService);
	EventHandler::Register();
	tlsRedisSystem.Initialize();

	GetKafkaManager().SetKafkaHandler(KafkaSystem::KafkaMessageHandler);
}



