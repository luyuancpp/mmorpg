#include "centre_node.h"

#include <ranges>
#include <grpcpp/grpcpp.h>

#include "handler/event/event_handler.h"
#include "handler/service/register_handler.h"
#include "handler/service/player/player_service.h"
#include "handler/service_replied/player/player_service_replied.h"
#include "muduo/net/EventLoop.h"
#include "network/rpc_session.h"
#include "node/centre_node_info.h"
#include "player/system/player_session_system.h"
#include "proto/logic/constants/node.pb.h"
#include "service_info/gate_service_service_info.h"
#include "thread_local/storage_centre.h"
#include "time/system/time_system.h"
#include "util/network_utils.h"

using namespace muduo;
using namespace net;

CentreNode* gCentreNode = nullptr;

muduo::AsyncLogging& logger()
{
	return  gCentreNode->Log();
}

CentreNode::CentreNode(muduo::net::EventLoop* loop)
	: Node(loop, "logs/centre"),
	redis_(std::make_shared<PbSyncRedisClientPtr::element_type>())
{
}

void CentreNode::Initialize()
{
	GetNodeInfo().set_node_type(CentreNodeService);

	gCentreNode = this;

	Node::Initialize();

	EventHandler::Register();

	InitPlayerService();
	InitPlayerServiceReplied();

	void InitServiceHandler();
	InitServiceHandler();
}

void CentreNode::StartRpcServer()
{
	rpcServer->registerService(&nodeReplyService);
	for (auto& val : gNodeService | std::views::values)
	{
		rpcServer->registerService(val.get());
	}

	Node::StartRpcServer();

	InitSystemAfterConnect();
	LOG_INFO << "centre start at " << GetNodeInfo().DebugString();
}

void CentreNode::PrepareForBeforeConnection()
{
	PlayerSessionSystem::Initialize();
}

void CentreNode::InitSystemAfterConnect() const
{
	InetAddress redisAddr("127.0.0.1", 6379);
	tls_centre.redis_system().Initialize(redisAddr);
}

