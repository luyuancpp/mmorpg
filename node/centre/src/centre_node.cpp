#include "centre_node.h"

#include <ranges>
#include <game_config/deploy_json.h>
#include <grpcpp/grpcpp.h>

#include "all_config.h"
#include "proto/common/deploy_service.grpc.pb.h"
#include "log/constants/log_constants.h"
#include "proto/logic/constants/node.pb.h"
#include "handler/event/event_handler.h"
#include "grpc/deploy/deploy_client.h"
#include "handler/service/player/player_service.h"
#include "handler/service/register_handler.h"
#include "muduo/base/TimeZone.h"
#include "muduo/net/EventLoop.h"
#include "network/rpc_session.h"
#include "handler/service_replied/player/player_service_replied.h"
#include "service_info/gate_service_service_info.h"
#include "service_info/service_info.h"
#include "thread_local/storage_centre.h"
#include "log/util/console_log_util.h"
#include "grpc/request/deploy_grpc_requst.h"
#include "game_logic/player/util/player_session_system.h"
#include "time/util/time_util.h"

using namespace muduo;
using namespace net;

CentreNode* gCentreNode = nullptr;

void InitRepliedHandler();
void AsyncCompleteGrpcDeployService();

void AsyncOutput(const char* msg, int len)
{
	gCentreNode->Log().append(msg, len);
#ifdef WIN32
	Log2Console(msg, len);
#endif
}

CentreNode::CentreNode(muduo::net::EventLoop* loop)
	: loop_(loop),
	muduoLog{ "logs/centre", kMaxLogFileRollSize, 1 },
	redis_(std::make_shared<PbSyncRedisClientPtr::element_type>())
{
}

CentreNode::~CentreNode()
{
	Exit();
}

void CentreNode::Init()
{
	gCentreNode = this;

	InitEventCallback();

	InitLog();
	EventHandler::Register();
	InitNodeConfig();

	nodeInfo.set_node_type(kCentreNode);
	nodeInfo.set_launch_time(TimeUtil::NowSecondsUTC());
	muduo::Logger::setLogLevel(static_cast <muduo::Logger::LogLevel> (
		ZoneConfig::GetSingleton().config_info().loglevel()));
	InitGameConfig();

	InitPlayerService();
	InitPlayerServiceReplied();
	InitRepliedHandler();
	InitMessageInfo();
	InitSystemBeforeConnect();

	InitNodeByReqInfo();

	void InitServiceHandler();
	InitServiceHandler();
}

void CentreNode::InitEventCallback()
{
	tls.dispatcher.sink<OnBeConnectedEvent>().connect<&CentreNode::Receive2>(*this);
}


void CentreNode::Exit()
{
	muduoLog.stop();
	tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&CentreNode::Receive2>(*this);
}

void CentreNode::InitGameConfig()
{
	LoadAllConfig();
	LoadAllConfigAsyncWhenServerLaunch();
	//ConfigSystem::OnConfigLoadSuccessful();
}

void CentreNode::InitTimeZone()
{
	const muduo::TimeZone tz("zoneinfo/Asia/Hong_Kong");
	muduo::Logger::setTimeZone(tz);
}

void CentreNode::InitNodeByReqInfo()
{
	const auto& deployInfo = DeployConfig::GetSingleton().deploy_info();
	const std::string targetStr = deployInfo.ip() + ":" + std::to_string(deployInfo.port());
	extern std::unique_ptr<DeployService::Stub> gDeployStub;
	gDeployStub = DeployService::NewStub(grpc::CreateChannel(targetStr, grpc::InsecureChannelCredentials()));
	gDeployCq = std::make_unique_for_overwrite<CompletionQueue>();

	deployRpcTimer.RunEvery(0.001, AsyncCompleteGrpcDeployService);

	{
		NodeInfoRequest request;
		request.set_zone_id(ZoneConfig::GetSingleton().config_info().zone_id());
		SendGetNodeInfo(request);
	}
}

void CentreNode::StartServer(const ::nodes_info_data& info)
{
	serversInfo = info;
	auto& myNodeInfo = serversInfo.centre_info().centre_info()[GetNodeConfIndex()];

	InetAddress serviceAddr(myNodeInfo.ip(), myNodeInfo.port());
	server_ = std::make_shared<RpcServerPtr::element_type>(loop_, serviceAddr);

	server_->registerService(&centreService);
	for (auto& value : g_server_service | std::views::values)
	{
		server_->registerService(value.get());
	}
	server_->start();
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

void CentreNode::SetNodeId(NodeId nodeId)
{
	nodeInfo.set_node_id(nodeId);
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

void CentreNode::InitLog()
{
	InitTimeZone();
	muduo::Logger::setOutput(AsyncOutput);
	muduoLog.start();
}

void CentreNode::InitConfig()
{

}

void CentreNode::InitNodeConfig()
{
	ZoneConfig::GetSingleton().Load("game.json");
	DeployConfig::GetSingleton().Load("deploy.json");
}

void CentreNode::InitSystemBeforeConnect()
{
	PlayerSessionSystem::Initialize();
}

void CentreNode::InitSystemAfterConnect() const
{
	InetAddress redisAddr(serversInfo.redis_info().redis_info(0).ip(),
		serversInfo.redis_info().redis_info(0).port());
	tls_centre.redis_system().Initialize(redisAddr);
}
