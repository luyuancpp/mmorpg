#include "centre_node.h"

#include <ranges>
#include <grpcpp/grpcpp.h>

#include "grpc/generator/deploy_service_grpc.h"
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

using namespace muduo;
using namespace net;

CentreNode* gCentreNode = nullptr;

void InitRepliedHandler();

muduo::AsyncLogging& logger()
{
	return  gCentreNode->Log();
}

CentreNode::CentreNode(muduo::net::EventLoop* loop)
	: Node(loop, "logs/centre"),
	redis_(std::make_shared<PbSyncRedisClientPtr::element_type>())
{
}

CentreNode::~CentreNode()
{
	ShutdownNode();
}

void CentreNode::Init()
{
	gCentreNode = this;

    GetNodeInfo().set_node_type(kCentreNode);
    GetNodeInfo().set_launch_time(TimeUtil::NowSecondsUTC());

	Node::Init();

	InitEventCallback();

	EventHandler::Register();

	InitPlayerService();
	InitPlayerServiceReplied();
	InitRepliedHandler();

	void InitServiceHandler();
	InitServiceHandler();
}

void CentreNode::InitEventCallback()
{
	tls.dispatcher.sink<OnBeConnectedEvent>().connect<&CentreNode::Receive2>(*this);
}

void CentreNode::ShutdownNode()
{
	Node::ShutdownNode();
	tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&CentreNode::Receive2>(*this);
}

NodeInfo& CentreNode::GetNodeInfo()
{
	return gCentreNodeInfo.GetNodeInfo();
}

uint32_t CentreNode::GetNodeType() const
{
	return kCentreNode;
}

void CentreNode::InitializeGameConfig()
{
	Node::InitializeGameConfig();
	//ConfigSystem::OnConfigLoadSuccessful();
}

void CentreNode::StartRpcServer(const ::nodes_info_data& info)
{
	serversInfo = info;
	auto& myNodeInfo = serversInfo.centre_info().centre_info()[GetNodeId()];

	InetAddress serviceAddr(myNodeInfo.ip(), myNodeInfo.port());
	rpcServer = std::make_unique<RpcServerPtr::element_type>(loop_, serviceAddr);

	rpcServer->registerService(&centreService);
	for (auto& value : g_server_service | std::views::values)
	{
		rpcServer->registerService(value.get());
	}
	rpcServer->start();
	deployRpcTimer.Cancel();
	InitSystemAfterConnect();
	LOG_INFO << "centre start " << myNodeInfo.DebugString();
}


void CentreNode::BroadCastRegisterGameToGate(entt::entity gameNodeId, entt::entity gate)
{
	auto gateNode = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate);
	if (nullptr == gateNode)
	{
		LOG_ERROR << "gate not found ";
		return;
	}
	auto gameNodeServiceAddr = tls.gameNodeRegistry.try_get<InetAddress>(gameNodeId);
	if (nullptr == gameNodeServiceAddr)
	{
		LOG_ERROR << "game not found ";
		return;
	}
	RegisterGameNodeRequest request;
	request.mutable_rpc_server()->set_ip(gameNodeServiceAddr->toIp());
	request.mutable_rpc_server()->set_port(gameNodeServiceAddr->port());
	request.set_game_node_id(entt::to_integral(gameNodeId));
	(*gateNode)->SendRequest(GateServiceRegisterGameMessageId, request);
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
		for (const auto& [e, gameNode] : tls.gameNodeRegistry.view<RpcSessionPtr>().each())
		{
			// 如果是游戏逻辑服则删除
			if (gameNode->connection->peerAddress().toIpPort() == currentAddr.toIpPort())
			{
				Destroy(tls.gameNodeRegistry, e);
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

void CentreNode::InitializeSystemBeforeConnection()
{
	PlayerSessionSystem::Initialize();
}

void CentreNode::InitSystemAfterConnect() const
{
	InetAddress redisAddr(serversInfo.redis_info().redis_info(0).ip(),
		serversInfo.redis_info().redis_info(0).port());
	tls_centre.redis_system().Initialize(redisAddr);
}

