﻿#include "centre_node.h"

#include <ranges>
#include <grpcpp/grpcpp.h>

#include "handler/event/event_handler.h"
#include "muduo/net/EventLoop.h"
#include "network/rpc_session.h"
#include "proto/common/node.pb.h"
#include "thread_local/storage_centre.h"
#include "kafka/system/kafka_system.h"

using namespace muduo;
using namespace net;

CentreNode::CentreNode(muduo::net::EventLoop* loop)
	: Node(loop, "logs/centre"){
	GetNodeInfo().set_node_type(CentreNodeService);
	EventHandler::Register();
	tls_centre.GetRedisSystem().Initialize();
	kafkaConsumerHandler = KafkaSystem::KafkaMessageHandler;

}



