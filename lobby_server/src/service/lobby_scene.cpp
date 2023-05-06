#include "lobby_scene.h"
#include "src/network/rpc_msg_route.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"

#include "src/game_config/mainscene_config.h"

#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/server_component.h"
#include "src/network/gs_node.h"
#include "src/network/controller_node.h"

#include <unordered_map>

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/tips_id.h"
#include "src/pb/pbc/controller_service_service.h"

#include "game_service.pb.h"
#include "controller_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using PlayerListMap = std::unordered_map<Guid, entt::entity>;
PlayerListMap  players_;

void AddCrossScene2Controller(uint32_t controller_node_id)
{
	auto controller_node_it = g_controller_nodes->find(controller_node_id);
	if (controller_node_it == g_controller_nodes->end())
	{
		LOG_ERROR << "controller not found " << controller_node_id;
		return;
	}
    AddCrossServerSceneRequest rq;
    for (auto e : tls.registry.view<MainScene>())
    {
        auto p_cross_scene_info = rq.mutable_cross_scenes_info()->Add();
        p_cross_scene_info->mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfo>(e));
        auto try_gs_node_ptr = tls.registry.try_get<GsNodePtr>(e);
        if (nullptr == try_gs_node_ptr)
        {
            continue;
        }
        p_cross_scene_info->set_gs_node_id((*try_gs_node_ptr)->node_id());
    }
	tls.registry.get<ControllerNodePtr>(controller_node_it->second)->session_.CallMethod(ControllerServiceAddCrossServerSceneMethod, &rq);
	LOG_DEBUG << rq.DebugString();
}


///<<< END WRITING YOUR CODE

///<<<rpc begin
void LobbyServiceImpl::StartCrossGs(::google::protobuf::RpcController* controller,
    const ::StartCrossGsRequest* request,
    ::StartCrossGsResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	//只有跨服gs能连
	
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress service_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity gs{ entt::null };
	for (auto e : tls.registry.view<RpcServerConnection>())
	{
		if (tls.registry.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != session_addr.toIpPort())
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

	auto c = tls.registry.get<RpcServerConnection>(gs);
	GsNodePtr gs_node_ptr = std::make_shared<GsNodePtr::element_type>(c.conn_);
	gs_node_ptr->node_info_.set_node_id(request->gs_node_id());
	gs_node_ptr->node_info_.set_node_type(kGameNode);
	AddMainSceneNodeCompnent(gs);
	tls.registry.emplace<InetAddress>(gs, service_addr);
	tls.registry.emplace<GsNodePtr>(gs, gs_node_ptr);
	if (request->server_type() == kMainSceneCrossServer)
	{
        tls.registry.remove<MainSceneServer>(gs);
        tls.registry.emplace<CrossMainSceneServer>(gs);

		auto& config_all = mainscene_config::GetSingleton().all();
		CreateGsSceneP create_scene_param;
		create_scene_param.node_ = gs;
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			create_scene_param.scene_confid_ = config_all.data(i).id();
			auto scene = ScenesSystem::CreateScene2Gs(create_scene_param);
			tls.registry.emplace<GsNodePtr>(scene, gs_node_ptr);
			response->add_scenes_info()->CopyFrom(tls.registry.get<SceneInfo>(scene));
		}
	}
	else
	{
		tls.registry.remove<MainSceneServer>(gs);
		tls.registry.emplace<CrossRoomSceneServer>(gs);
	}
	g_game_node->emplace(request->gs_node_id(), gs);

	LOG_INFO << "game node connected " << response->DebugString();
///<<< END WRITING YOUR CODE
}

void LobbyServiceImpl::StartControllerNode(::google::protobuf::RpcController* controller,
    const ::StartControllerRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress service_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity controller_node{ entt::null };
	for (auto e : tls.registry.view<RpcServerConnection>())
	{
		if (tls.registry.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		controller_node = e;
		break;
	}
	if (controller_node == entt::null)
	{
		//todo
		LOG_INFO << "ms id found: " << request->controller_node_id();
		return;
	}

	auto c = tls.registry.get<RpcServerConnection>(controller_node);
	ControllerNodePtr controller_node_ptr = tls.registry.emplace<ControllerNodePtr>(controller_node, std::make_shared<ControllerNodePtr::element_type>(c.conn_));
	controller_node_ptr->node_info_.set_node_id(request->controller_node_id());
	controller_node_ptr->node_info_.set_node_type(kControllerNode);
	tls.registry.emplace<InetAddress>(controller_node, service_addr);
	g_controller_nodes->emplace(request->controller_node_id(), controller_node);

	//todo next frame send after responese
	AddCrossScene2Controller(request->controller_node_id());
///<<< END WRITING YOUR CODE
}

void LobbyServiceImpl::EnterCrossMainScene(::google::protobuf::RpcController* controller,
    const ::EnterCrossMainSceneRequest* request,
    ::EnterCrossMainSceneResponese* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	auto scene = ScenesSystem::get_scene(request->scene_id());
	if (entt::null == scene)
	{
		response->mutable_error()->set_id(kRetEnterScenetWeightRoundRobinMainScene);
		return;
	}
	auto it = players_.find(request->player_id());
	if (it == players_.end())
	{
		auto result = players_.emplace(request->player_id(), tls.registry.create());
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

	auto ret = ScenesSystem::CheckScenePlayerSize(scene);
	if (ret != kRetOK)
	{
		response->mutable_error()->set_id(ret);		
		return;
	}
	//disconnect scene 
	//todo lobby not has scene 

    //todo error
    LeaveSceneParam lsp;
    lsp.leaver_ = player;
    ScenesSystem::LeaveScene(lsp);

	EnterSceneParam esp;
	esp.scene_ = scene;
	esp.enterer_ = player;
	ScenesSystem::EnterScene(esp);

	response->set_player_id(request->player_id());
	response->set_scene_id(request->scene_id());
///<<< END WRITING YOUR CODE
}

void LobbyServiceImpl::EnterCrossMainSceneWeightRoundRobin(::google::protobuf::RpcController* controller,
    const ::EnterCrossMainSceneWeightRoundRobinRequest* request,
    ::EnterCrossRoomSceneSceneWeightRoundRobinResponse* response,
    ::google::protobuf::Closure* done)
{
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
	auto it = players_.emplace(request->player_id(), tls.registry.create());
	if (!it.second)
	{
		response->mutable_error()->set_id(kRetEnterSceneCreatePlayer);
		LOG_ERROR << "EnterCrossMainScene" << request->player_id();
		return;
	}
	ReturnAutoCloseureError(ScenesSystem::CheckScenePlayerSize(scene));
	auto player = it.first->second;

	//todo error
    LeaveSceneParam lsp;
    lsp.leaver_ = player;
    ScenesSystem::LeaveScene(lsp);

	EnterSceneParam esp;
	esp.scene_ = scene;
	esp.enterer_ = player;
	ScenesSystem::EnterScene(esp);
///<<< END WRITING YOUR CODE
}

void LobbyServiceImpl::LeaveCrossMainScene(::google::protobuf::RpcController* controller,
    const ::LeaveCrossMainSceneRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    auto it = players_.find(request->player_id());
    if (it == players_.end())
    {
        return;
    }
    auto player = it->second;;
    LeaveSceneParam lsp;
    lsp.leaver_ = player;
    ScenesSystem::LeaveScene(lsp);
	players_.erase(it);
///<<< END WRITING YOUR CODE
}

void LobbyServiceImpl::GameConnectToController(::google::protobuf::RpcController* controller,
    const ::GameConnectToControllerRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    for (auto& mit : *g_controller_nodes)
    {
        AddCrossScene2Controller(mit.first);
    }
///<<< END WRITING YOUR CODE
}

///<<<rpc end
