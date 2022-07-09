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

using namespace msservice;

using GsStubPtr = std::unique_ptr <RpcStub<gsservice::GsService_Stub>>;
using MsStubPtr = std::unique_ptr <RpcStub<msservice::MasterNodeService_Stub>>;

void AddCrossScene2Ms(uint32_t ms_node_id)
{
	auto ms_node_it = g_ms_nodes->find(ms_node_id);
	if (ms_node_it == g_ms_nodes->end())
	{
		LOG_ERROR << "ms not found " << ms_node_id;
		return;
	}
    AddCrossServerSceneRequest rpc;
    for (auto e : registry.view<MainScene>())
    {
        auto p_cross_scene_info = rpc.mutable_cross_scenes_info()->Add();
        p_cross_scene_info->mutable_scene_info()->CopyFrom(registry.get<SceneInfo>(e));
        auto try_gs_node_ptr = registry.try_get<GsNodePtr>(e);
        if (nullptr == try_gs_node_ptr)
        {
            continue;
        }
        p_cross_scene_info->set_gs_node_id((*try_gs_node_ptr)->node_id());
    }
	registry.get<MsStubPtr>(ms_node_it->second)->CallMethod(rpc, &msservice::MasterNodeService_Stub::AddCrossServerScene);;
}


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
	if (request->server_type() == kMainSceneCrossServer)
	{
        registry.remove<MainSceneServer>(gs);
        registry.emplace<CrossMainSceneServer>(gs);

		auto& config_all = mainscene_config::GetSingleton().all();
		CreateGsSceneP create_scene_param;
		create_scene_param.node_ = gs;
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			create_scene_param.scene_confid_ = config_all.data(i).id();
			auto scene = ScenesSystem::GetSingleton().CreateScene2Gs(create_scene_param);
			registry.emplace<GsNodePtr>(scene, gs_node_ptr);
			response->add_scenes_info()->CopyFrom(registry.get<SceneInfo>(scene));
		}
	}
	else
	{
		registry.remove<MainSceneServer>(gs);
		registry.emplace<CrossRoomSceneServer>(gs);
	}
	g_gs_nodes->emplace(request->gs_node_id(), gs);

	for (auto& mit : *g_ms_nodes)
	{
		AddCrossScene2Ms(mit.first);
	}
	LOG_INFO << "game node connected " << response->DebugString();
///<<< END WRITING YOUR CODE 
}

void RgServiceImpl::StartMs(::google::protobuf::RpcController* controller,
    const regionservcie::StartMsRequest* request,
    ::google::protobuf::Empty* response,
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
	g_ms_nodes->emplace(request->ms_node_id(), ms);

	auto& ms_stub = registry.emplace<MsStubPtr>(ms, std::make_unique<MsStubPtr::element_type>(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));

	//todo next frame send after responese
	AddCrossScene2Ms(request->ms_node_id());
///<<< END WRITING YOUR CODE 
}

void RgServiceImpl::EnterCrossMainScene(::google::protobuf::RpcController* controller,
    const regionservcie::EnterCrossMainSceneRequest* request,
    regionservcie::EnterCrossMainSceneResponese* response,
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
	auto it = players_.find(request->player_id());
	if (it == players_.end())
	{
		auto result = players_.emplace(request->player_id(), registry.create());
		if (!result.second)
		{
            response->mutable_error()->set_id(kRetEnterSceneCreatePlayer);
            LOG_ERROR << "EnterCrossMainScene" << request->player_id();
            return;
		}
		it = result.first;
    }
	if (it == players_.end())
	{
        response->mutable_error()->set_id(kRetEnterSceneCreatePlayer);
        LOG_ERROR << "EnterCrossMainScene" << request->player_id();
        return;
	}
    auto player = it->second;

    //原来就在跨服上面，先离开跨服场景
	//先离开，不然人数少个判断不了

    LeaveSceneParam lsp;
    lsp.leaver_ = player;
    ScenesSystem::GetSingleton().LeaveScene(lsp);

	auto ret = ScenesSystem::GetSingleton().CheckScenePlayerSize(scene);
	if (ret != kRetOK)
	{
		response->mutable_error()->set_id(ret);		
		return;
	}
	

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
	//todo 离开跨服马上删除
	auto it = players_.emplace(request->player_id(), registry.create());
	if (!it.second)
	{
		response->mutable_error()->set_id(kRetEnterSceneCreatePlayer);
		LOG_ERROR << "EnterCrossMainScene" << request->player_id();
		return;
	}
	ReturnAutoCloseureError(ScenesSystem::GetSingleton().CheckScenePlayerSize(scene));

	EnterSceneParam esp;
	esp.scene_ = scene;
	esp.enterer_ = it.first->second;
	ScenesSystem::GetSingleton().EnterScene(esp);
///<<< END WRITING YOUR CODE 
}

void RgServiceImpl::LeaveCrossMainScene(::google::protobuf::RpcController* controller,
    const regionservcie::LeaveCrossMainSceneRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
    auto it = players_.find(request->player_id());
    if (it == players_.end())
    {
        return;
    }
    auto player = it->second;;
    LeaveSceneParam lsp;
    lsp.leaver_ = player;
    ScenesSystem::GetSingleton().LeaveScene(lsp);
	players_.erase(it);
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
