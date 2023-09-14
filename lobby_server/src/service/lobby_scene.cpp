#include "lobby_scene.h"
#include "src/network/rpc_msg_route.h"
///<<< BEGIN WRITING YOUR CODE

#include <unordered_map>

#include "muduo/base/Logging.h"

#include "src/game_config/mainscene_config.h"

#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/scene/scene_system.h"
#include "src/network/server_component.h"
#include "src/network/game_node.h"
#include "src/thread_local/lobby_thread_local_storage.h"

#include "src/game_logic/tips_id.h"
#include "src/pb/pbc/controller_service_service.h"


using namespace muduo;
using namespace muduo::net;

using PlayerListMap = std::unordered_map<Guid, entt::entity>;
PlayerListMap  players_;

void AddCrossScene2Controller(uint32_t controller_node_id)
{
	auto controller_node_it = lobby_tls.controller_nodes().find(controller_node_id);
	if (controller_node_it == lobby_tls.controller_nodes().end())
	{
		LOG_ERROR << "controller not found " << controller_node_id;
		return;
	}
    AddCrossServerSceneRequest rq;
    for (auto e : tls.registry.view<SceneInfo>())
    {
        auto cross_scene_info = rq.mutable_cross_scenes_info()->Add();
        cross_scene_info->mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfo>(e));
        auto game_node_ptr = tls.registry.try_get<GameNodePtr>(e);
        if (nullptr == game_node_ptr)
        {
            continue;
        }
        cross_scene_info->set_gs_node_id((*game_node_ptr)->node_id());
    }
	controller_node_it->second->session_.Send(ControllerServiceAddCrossServerSceneMsgId, rq);
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
	GameNodePtr game_node_ptr = std::make_shared<GameNodePtr::element_type>(c.conn_);
	game_node_ptr->node_info_.set_node_id(request->gs_node_id());
	game_node_ptr->node_info_.set_node_type(kGameNode);
    AddMainSceneNodeComponent(gs);
	tls.registry.emplace<InetAddress>(gs, service_addr);
	tls.registry.emplace<GameNodePtr>(gs, game_node_ptr);
	if (request->server_type() == kMainSceneCrossServer)
	{
		tls.registry.remove<MainSceneServer>(gs);
		tls.registry.emplace<CrossMainSceneServer>(gs);
		const auto& config_all = mainscene_config::GetSingleton().all();
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			const auto scene = ScenesSystem::CreateScene2GameNode({.node_ = gs, .scene_config_id_ = config_all.data(i).id()});
			tls.registry.emplace<GameNodePtr>(scene, game_node_ptr);
			response->add_scenes_info()->CopyFrom(tls.registry.get<SceneInfo>(scene));
		}
	}
	else
	{
		tls.registry.remove<MainSceneServer>(gs);
		tls.registry.emplace<CrossRoomSceneServer>(gs);
	}
	lobby_tls.game_node_list().emplace(request->gs_node_id(), gs);

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
		LOG_INFO << "controller id found: " << request->controller_node_id();
		return;
	}

	auto& c = tls.registry.get<RpcServerConnection>(controller_node);
	ControllerNodePtr controller_node_ptr = tls.registry.emplace<ControllerNodePtr>(controller_node, std::make_shared<ControllerNodePtr::element_type>(c.conn_));
	controller_node_ptr->node_info_.set_node_id(request->controller_node_id());
	controller_node_ptr->node_info_.set_node_type(kControllerNode);
	tls.registry.emplace<InetAddress>(controller_node, service_addr);
	lobby_tls.controller_nodes().emplace(request->controller_node_id(), controller_node_ptr);

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
	auto scene = ScenesSystem::GetSceneByGuid(request->scene_id());
	if (entt::null == scene)
	{
		response->mutable_error()->set_id(kRetEnterSceneWeightRoundRobinMainScene);
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
	
	ScenesSystem::EnterScene({scene, player});

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
	weight_round_robin_scene.scene_conf_id_ = request->scene_confid();
	auto scene = ServerNodeSystem::GetSceneOnMinPlayerSizeNode(weight_round_robin_scene);
	if (entt::null == scene)
	{
		response->mutable_error()->set_id(kRetEnterSceneWeightRoundRobinMainScene);
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
	ReturnAutoClosureError(ScenesSystem::CheckScenePlayerSize(scene));
	auto player = it.first->second;

	//todo error
ScenesSystem::LeaveScene({player});
	ScenesSystem::EnterScene({scene, player});
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
    for (auto& mit : lobby_tls.controller_nodes())
    {
        AddCrossScene2Controller(mit.first);
    }
///<<< END WRITING YOUR CODE
}

///<<<rpc end
