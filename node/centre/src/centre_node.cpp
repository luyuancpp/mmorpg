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
	gCentreNode = this;

	Node::Initialize();

	InitEventCallback();

	EventHandler::Register();

	InitPlayerService();
	InitPlayerServiceReplied();

	void InitServiceHandler();
	InitServiceHandler();
}

void CentreNode::InitEventCallback()
{
	tls.dispatcher.sink<OnBeConnectedEvent>().connect<&CentreNode::Receive2>(*this);
}

NodeInfo& CentreNode::GetNodeInfo()
{
	return gCentreNodeInfo.GetNodeInfo();
}

uint32_t CentreNode::GetNodeType() const
{
	return kCentreNode;
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

void CentreNode::Receive2(const OnBeConnectedEvent& es)
{
	if (auto& conn = es.conn_; conn->connected())
	{
		const auto e = tls.networkRegistry.create();
		tls.networkRegistry.emplace<RpcSession>(e, RpcSession{ conn });
	}
	else
	{
		auto& currentAddr = conn->peerAddress();
		for (const auto& [e, gameNode] : tls.sceneNodeRegistry.view<RpcSessionPtr>().each())
		{
			// 如果是游戏逻辑服则删除
			if (gameNode->connection->peerAddress().toIpPort() == currentAddr.toIpPort())
			{
				Destroy(tls.sceneNodeRegistry, e);
				break;
			}
		}

		for (const auto& [e, gateNode] : tls.gateNodeRegistry.view<RpcSessionPtr>().each())
		{
			// 如果是游戏逻辑服则删除
			if (nullptr != gateNode &&
				gateNode->connection->peerAddress().toIpPort() == currentAddr.toIpPort())
			{
				// remove AfterChangeGsEnterScene
				// todo 
				Destroy(tls.gateNodeRegistry, e);
				break;
			}
		}

		for (const auto& [e, session] : tls.networkRegistry.view<RpcSession>().each())
		{
			if (session.connection->peerAddress().toIpPort() != currentAddr.toIpPort())
			{
				continue;
			}
			Destroy(tls.networkRegistry, e);
			break;
		}
	}
}

void CentreNode::PrepareForBeforeConnection()
{
	PlayerSessionSystem::Initialize();
}

std::string CentreNode::GetServiceName() const
{
	return "centreservcie.rpc";
}

void CentreNode::InitSystemAfterConnect() const
{
	InetAddress redisAddr("127.0.0.1", 6379);
	tls_centre.redis_system().Initialize(redisAddr);
}

