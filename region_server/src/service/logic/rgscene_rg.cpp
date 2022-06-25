#include "rgscene_rg.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"


#include "src/game_config/mainscene_config.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/scene_factories.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/server_component.h"
#include "src/network/gs_node.h"
#include "src/network/ms_node.h"
#include "src/game_logic/tips_id.h"

#include "gs_service.pb.h"
#include "ms_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using GsStubPtr = std::unique_ptr <RpcStub<gsservice::GsService_Stub>>;
using MsStubPtr = std::unique_ptr <RpcStub<msservice::MasterNodeService_Stub>>;
///<<< END WRITING YOUR CODE

///<<<rpc begin
void RgServiceImpl::StartCrossGs(::google::protobuf::RpcController* controller,
    const regionservcie::StartCrossGsRequest* request,
    regionservcie::StartCrossGsResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	//只有跨服gs能连
	if (!(request->server_type() == kMainSceneCrossServer ||
		request->server_type() == kRoomSceneCrossServer))
	{
		LOG_ERROR << "server type " << request->rpc_server().ip() << "," << request->rpc_server().port();
		return;
	}
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress service_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity gs{ entt::null };
	for (auto e : registry.view<RpcServerConnection>())
	{
		if (registry.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		gs = e;
		break;
	}
	if (gs == entt::null)
	{
		//todo
		LOG_INFO << "game connection not found " << request->gs_node_id();
		return;
	}

	auto c = registry.get<RpcServerConnection>(gs);
	GsNodePtr gs_node_ptr = std::make_shared<GsNodePtr::element_type>(c.conn_);
	gs_node_ptr->node_info_.set_node_id(request->gs_node_id());
	gs_node_ptr->node_info_.set_node_type(kGsNode);
	AddMainSceneNodeCompnent(gs);
	registry.emplace<InetAddress>(gs, service_addr);
	registry.emplace<GsNodePtr>(gs, gs_node_ptr);
	registry.emplace<GsStubPtr>(gs, std::make_unique<GsStubPtr::element_type>(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));
	if (request->server_type() == kMainSceneServer)
	{
		auto& config_all = mainscene_config::GetSingleton().all();
		MakeGsSceneP create_scene_param;
		create_scene_param.node_ = gs;
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			create_scene_param.scene_confid_ = config_all.data(i).id();
			auto scene = ScenesSystem::GetSingleton().MakeScene2Gs(create_scene_param);
			registry.remove<MainSceneServer>(gs);
			registry.emplace<CrossMainSceneServer>(gs);
			registry.emplace<GsNodePtr>(scene);
			response->add_scenes_info()->CopyFrom(registry.get<SceneInfo>(scene));
		}
	}
	else
	{
		registry.remove<MainSceneServer>(gs);
		registry.emplace<CrossRoomSceneServer>(gs);
	}
	g_gs_nodes->emplace(request->gs_node_id(), gs);
	LOG_INFO << "game node connected " << request->gs_node_id();
///<<< END WRITING YOUR CODE 
}

void RgServiceImpl::StartMs(::google::protobuf::RpcController* controller,
    const regionservcie::StartMsRequest* request,
    regionservcie::StartMsResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress service_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity ms{ entt::null };
	for (auto e : registry.view<RpcServerConnection>())
	{
		if (registry.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		ms = e;
		break;
	}
	if (ms == entt::null)
	{
		//todo
		LOG_INFO << "ms id found: " << request->ms_node_id();
		return;
	}

	auto c = registry.get<RpcServerConnection>(ms);
	MsNodePtr ms_node = registry.emplace<MsNodePtr>(ms, std::make_shared<MsNodePtr::element_type>(c.conn_));
	ms_node->node_info_.set_node_id(request->ms_node_id());
	ms_node->node_info_.set_node_type(kMasterNode);
	registry.emplace<InetAddress>(ms, service_addr);
	registry.emplace<MsStubPtr>(ms, std::make_unique<MsStubPtr::element_type>(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));
	g_ms_nodes->emplace(request->ms_node_id(), ms);
	LOG_INFO << "ms node connected " << request->ms_node_id();
///<<< END WRITING YOUR CODE 
}

void RgServiceImpl::EnterCrossMainScene(::google::protobuf::RpcController* controller,
    const regionservcie::EnterCrossMainSceneRequest* request,
    regionservcie::EnterCrossRoomSceneSceneResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto scene = ScenesSystem::GetSingleton().get_scene(request->scene_id());
	if (entt::null == scene)
	{
		response->mutable_error()->set_id(kRetEnterScenetWeightRoundRobinMainScene);
		return;
	}
	auto it = players_.emplace(request->player_id(), registry.create());
	if (!it.second)
	{
		response->mutable_error()->set_id(kRetEnterSceneCreatePlayer);
		LOG_ERROR << "EnterCrossMainScene" << request->player_id();
		return;
	}
	auto player = it.first->second;;
	CheckEnterSceneParam csp;
	csp.scene_id_ = registry.get<Guid>(scene);
	csp.player_ = player;
	ReturnAutoCloseureError(ScenesSystem::GetSingleton().CheckScenePlayerSize(csp));

	EnterSceneParam esp;
	esp.scene_ = scene;
	esp.enterer_ = player;
	ScenesSystem::GetSingleton().EnterScene(esp);
///<<< END WRITING YOUR CODE 
}

void RgServiceImpl::EnterCrossMainSceneWeightRoundRobin(::google::protobuf::RpcController* controller,
    const regionservcie::EnterCrossMainSceneWeightRoundRobinRequest* request,
    regionservcie::EnterCrossRoomSceneSceneWeightRoundRobinResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	GetSceneParam weight_round_robin_scene;
	weight_round_robin_scene.scene_confid_ = request->scene_confid();
	auto scene = ServerNodeSystem::GetSingleton().GetWeightRoundRobinMainScene(weight_round_robin_scene);
	if (entt::null == scene)
	{
		response->mutable_error()->set_id(kRetEnterScenetWeightRoundRobinMainScene);
		return;
	}
	auto it = players_.emplace(request->player_id(), registry.create());
	if (!it.second)
	{
		response->mutable_error()->set_id(kRetEnterSceneCreatePlayer);
		LOG_ERROR << "EnterCrossMainScene" << request->player_id();
		return;
	}
	CheckEnterSceneParam csp;
	csp.scene_id_ = registry.get<Guid>(scene);
	csp.player_ = it.first->second;
	ReturnAutoCloseureError(ScenesSystem::GetSingleton().CheckScenePlayerSize(csp));

	EnterSceneParam esp;
	esp.scene_ = scene;
	esp.enterer_ = it.first->second;
	ScenesSystem::GetSingleton().EnterScene(esp);
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
