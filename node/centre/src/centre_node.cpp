#include "centre_node.h"

#include <ranges>
#include <grpcpp/grpcpp.h>

#include "handler/event/event_handler.h"
#include "service/player_service.h"
#include "service/player_service_replied.h"
#include "muduo/net/EventLoop.h"
#include "network/rpc_session.h"
#include "player/system/player_session_system.h"
#include "proto/logic/constants/node.pb.h"
#include "thread_local/storage_centre.h"

using namespace muduo;
using namespace net;

CentreNode* gCentreNode = nullptr;

CentreNode::CentreNode(muduo::net::EventLoop* loop)
	: Node(loop, "logs/centre")
{
	gCentreNode = this;
	GetNodeInfo().set_node_type(CentreNodeService);

	EventHandler::Register();

}

void CentreNode::StartRpcServer()
{
	tls_centre.GetRedisSystem().Initialize();

	Node::StartRpcServer();

	LOG_INFO << "centre start at " << GetNodeInfo().DebugString();
}

void CentreNode::InitGlobalData()
{
	Node::InitGlobalData();
	PlayerSessionSystem::Initialize();
}


