#include "controller_service.h"
#include "src/network/rpc_msg_route.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/net/InetAddress.h"

#include "src/game_config/mainscene_config.h"

#include "src/common_type/common_type.h"
#include "src/comp/account_player.h"
#include "src/controller_server.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/util/game_registry.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/comp/player_list.h"
#include "src/game_logic/comp/account_comp.h"
#include "src/game_logic/tips_id.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/network/message_system.h"
#include "src/network/gate_session.h"
#include "src/network/session.h"
#include "src/network/gate_node.h"
#include "src/network/gs_node.h"
#include "src/network/player_session.h"
#include "src/network/server_component.h"
#include "src/network/node_info.h"
#include "src/pb/pbc/service_method/gate_servicemethod.h"
#include "src/pb/pbc/service_method/game_servicemethod.h"
#include "src/pb/pbc/serviceid/gateservice_service_method_id.h"
#include "src/pb/pbc/serviceid/service_method_id.h"
#include "src/service/logic_proto/player_service.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_common_system.h"
#include "src/system/player_change_scene.h"
#include "src/thread_local/controller_thread_local_storage.h"


#include "component_proto/player_comp.pb.h"
#include "component_proto/player_login_comp.pb.h"
#include "game_service.pb.h"
#include "gate_service.pb.h"
#include "logic_proto/common_server_player.pb.h"
#include "logic_proto/scene.pb.h"

using AccountSessionMap = std::unordered_map<std::string, uint64_t>;
AccountSessionMap logined_accounts_sesion_;

std::size_t kMaxPlayerSize = 1000;

Guid GetPlayerIdByConnId(uint64_t session_id)
{
	auto cit = controller_tls.gate_sessions().find(session_id);
	if (cit == controller_tls.gate_sessions().end())
	{
		return kInvalidGuid;
	}
	auto p_try_player = tls.registry.try_get<EntityPtr>(cit->second);
	if (nullptr == p_try_player)
	{
		return kInvalidGuid;
	}
	auto player_id = tls.registry.get<Guid>(*p_try_player);
	return kInvalidGuid;
}

entt::entity GetPlayerByConnId(uint64_t session_id)
{
	auto cit = controller_tls.gate_sessions().find(session_id);
	if (cit == controller_tls.gate_sessions().end())
	{
		return entt::null;
	}
	auto p_try_player = tls.registry.try_get<EntityPtr>(cit->second);
	if (nullptr == p_try_player)
	{
		return entt::null;
	}
	return (*p_try_player);
}

void OnSessionEnterGame(entt::entity conn, Guid player_id)
{
    tls.registry.emplace<EntityPtr>(conn, ControllerPlayerSystem::GetPlayerPtr(player_id));
    tls.registry.emplace<Guid>(conn, player_id);
}

void InitPlayerGate(entt::entity player, uint64_t session_id)
{
    auto& player_session = tls.registry.get_or_emplace<PlayerSession>(player);
    player_session.gate_session_.set_session_id(session_id);
    auto gate_it = controller_tls.gate_nodes().find(node_id(session_id));
    if (gate_it == controller_tls.gate_nodes().end())
    {
		return;  
    }
	player_session.gate_ = gate_it->second;
}

///<<< END WRITING YOUR CODE

///<<<rpc begin
void ControllerServiceImpl::StartGs(::google::protobuf::RpcController* controller,
    const ::ControllerNodeStartGsRequest* request,
    ::ControllerNodeStartGsResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	response->set_controller_node_id(controller_node_id());
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
	tls.registry.emplace<InetAddress>(gs, service_addr);//为了停掉gs，或者gs断线用
	tls.registry.emplace<GsNodePtr>(gs, gs_node_ptr);
	if (request->server_type() == kMainSceneServer)
	{
		auto& config_all = mainscene_config::GetSingleton().all();
		CreateGsSceneP create_scene_param;
		create_scene_param.node_ = gs;
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			create_scene_param.scene_confid_ = config_all.data(i).id();
			auto scene_entity = ScenesSystem::CreateScene2Gs(create_scene_param);
			tls.registry.emplace<GsNodePtr>(scene_entity, gs_node_ptr);
			response->add_scenes_info()->CopyFrom(tls.registry.get<SceneInfo>(scene_entity));
		}
	}
	else if (request->server_type() == kMainSceneCrossServer)
	{
        tls.registry.remove<MainSceneServer>(gs);
        tls.registry.emplace<CrossMainSceneServer>(gs);
	}
    else if (request->server_type() == kRoomSceneCrossServer)
    {
        tls.registry.remove<MainSceneServer>(gs);
        tls.registry.emplace<CrossRoomSceneServer>(gs);
    }
	else
	{
		tls.registry.remove<MainSceneServer>(gs);
		tls.registry.emplace<RoomSceneServer>(gs);
	}

	for (auto e : tls.registry.view<GateNodePtr>())
	{
		g_controller_node->LetGateConnect2Gs(gs, e);
	}
	controller_tls.game_node().emplace(request->gs_node_id(), gs);
	LOG_DEBUG << "gs connect node id: " << request->gs_node_id() << response->DebugString() << "server type:" << request->server_type();
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::OnGateConnect(::google::protobuf::RpcController* controller,
    const ::ControllerNodeConnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	entt::entity gate{ entt::null };
	for (auto e : tls.registry.view<RpcServerConnection>())
	{
		auto c = tls.registry.get<RpcServerConnection>(e);
		auto& local_addr = c.conn_->peerAddress();
		if (local_addr.toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		gate = e;
		auto& gate_node = tls.registry.emplace<GateNodePtr>(gate, std::make_shared<GateNodePtr::element_type>(c.conn_));
		gate_node->node_info_.set_node_id(request->gate_node_id());
		gate_node->node_info_.set_node_type(kGateNode);
		gate_node->entity_id_ = e;
		controller_tls.gate_nodes().emplace(request->gate_node_id(), gate_node);
		break;
	}
	tls.registry.emplace<InetAddress>(gate, session_addr);
	for (auto e : tls.registry.view<GsNodePtr>())
	{
		g_controller_node->LetGateConnect2Gs(e, gate);
	}
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::OnGateLeaveGame(::google::protobuf::RpcController* controller,
    const ::ControllerNodeLeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::OnGatePlayerService(::google::protobuf::RpcController* controller,
    const ::ControllerNodeClientMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::OnGateDisconnect(::google::protobuf::RpcController* controller,
    const ::ControllerNodeDisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 

	//断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
	auto player = GetPlayerByConnId(request->session_id());
	if (entt::null == player)
	{
		return;
	}
	auto try_acount = tls.registry.try_get<PlayerAccount>(player);
	if (nullptr != try_acount)
	{
		logined_accounts_sesion_.erase(**try_acount);
	}	
	auto try_player_session = tls.registry.try_get<PlayerSession>(player);
	if (nullptr == try_player_session)//玩家已经断开连接了
	{
		return;
	}
	//notice 异步过程 gate 先重连过来，然后断开才收到，也就是会把新来的连接又断了，极端情况，也要测试如果这段代码过去了，会有什么问题
	if (try_player_session->session_id() != request->session_id())
	{
		return;
	}
	auto it = controller_tls.game_node().find(try_player_session->gs_node_id());
	if (it == controller_tls.game_node().end())
	{
		return;
	}
	auto player_id = tls.registry.get<Guid>(player);
	controller_tls.gate_sessions().erase(player_id);
	GameNodeDisconnectRequest rq;
	rq.set_player_id(player_id);
	tls.registry.get<GsNodePtr>(it->second)->session_.CallMethod(GameServiceDisconnect, &rq);
	ControllerPlayerSystem::LeaveGame(player_id);
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::OnLsLoginAccount(::google::protobuf::RpcController* controller,
    const ::ControllerNodeLoginAccountRequest* request,
    ::ControllerNodeLoginAccountResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 

	auto cit = controller_tls.gate_sessions().find(request->session_id());
	if (cit == controller_tls.gate_sessions().end())
	{
		cit = controller_tls.gate_sessions().emplace(request->session_id(), EntityPtr()).first;
	}
	if (cit == controller_tls.gate_sessions().end())
	{
        response->mutable_error()->set_id(kRetLoginUnkonwError);
        return;
	}
	auto conn = cit->second;
    tls.registry.emplace<PlayerAccount>(conn, std::make_shared<PlayerAccount::element_type>(request->account()));
    tls.registry.emplace<AccountLoginNode>(conn, AccountLoginNode{request->session_id()});
	//todo 
	auto lit = logined_accounts_sesion_.find(request->account());
	if (controller_tls.player_list().size() >= kMaxPlayerSize)
	{
		//如果登录的是新账号,满了得去排队,是账号排队，还是角色排队>???
		response->mutable_error()->set_id(kRetLoginAccountPlayerFull);
		return;
	}

	if (lit != logined_accounts_sesion_.end())
	{
		//如果不是同一个登录服务器,踢掉已经登录的账号
		if (lit->second != request->session_id())
		{

		}
		else//告诉客户端登录中
		{
			response->mutable_error()->set_id(kRetLoginIng);
		}
	}
	else
	{
		logined_accounts_sesion_.emplace(request->account(), request->session_id());
	}
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::OnLsEnterGame(::google::protobuf::RpcController* controller,
    const ::ControllerNodeEnterGameRequest* request,
    ::ControllerNodeEnterGameResponese* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	//todo正常或者顶号进入场景
	//todo 断线重连进入场景，断线重连分时间
    auto sit = controller_tls.gate_sessions().find(request->session_id());
	if (sit == controller_tls.gate_sessions().end())
	{
		LOG_ERROR << "connection not found " << request->session_id();
		return;
	}
	auto session = sit->second;
	auto player_id = request->player_id();
	auto player = ControllerPlayerSystem::GetPlayer(player_id);
	auto try_acount = tls.registry.try_get<PlayerAccount>(session);
	if (nullptr != try_acount)
	{
		logined_accounts_sesion_.erase(**try_acount);
	}
	if (entt::null == player)
	{
		//把旧的connection 断掉
		player = ControllerPlayerSystem::EnterGame(player_id);
		PlayerCommonSystem::InitPlayerCompnent(player);
		OnSessionEnterGame(session, player_id);
		tls.registry.emplace<Guid>(player, player_id);
		
		tls.registry.emplace<PlayerAccount>(player, tls.registry.get<PlayerAccount>(sit->second));
		
		GetSceneParam getp;
		getp.scene_confid_ = 1;
		auto scene = ServerNodeSystem::GetMainSceneNotFull(getp);
		if (scene == entt::null)//找不到上次的场景，放到默认场景里面
		{
			// todo default
			LOG_INFO << "player " << player_id << " enter default secne";
		} 
		
	
		InitPlayerGate(player, request->session_id());
		tls.registry.emplace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_FIRST);

        auto try_player_session = tls.registry.try_get<PlayerSession>(player);
        if (nullptr == try_player_session)
        {
            LOG_ERROR << "enter scene not found or destroy" << tls.registry.get<Guid>(player);
			// to do 让人下线
            return;

        }
		PlayerSceneSystem::CallPlayerEnterGs(player, PlayerSceneSystem::GetGsNodeIdByScene(scene), try_player_session->session_id());
        ControllerChangeSceneInfo change_scene_info;
        change_scene_info.mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfo>(scene));
        change_scene_info.set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);
        change_scene_info.set_change_gs_status(ControllerChangeSceneInfo::eEnterGsSceneSucceed);
        PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_scene_info);        
	}
	else//顶号,断线重连 记得gate 删除 踢掉老gate,但是是同一个gate就不用了
	{
		//顶号的时候已经在场景里面了,不用再进入场景了
		//todo换场景的过程中被顶了
		
		//告诉账号被顶
		OnSessionEnterGame(session, player_id);
        //断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
        auto player_session = tls.registry.try_get<PlayerSession>(player);
        if (nullptr != player_session)
        {
			GateNodeKickConnRequest message;
            message.set_session_id(player_session->gate_session_.session_id());
            Send2Gate(GateService_Id_KickConnByController, message, player_session->gate_node_id());
        }
		InitPlayerGate(player, request->session_id());
		tls.registry.emplace_or_replace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_REPLACE);//连续顶几次,所以用emplace_or_replace
	}
	if (entt::null == player)
	{
		LOG_ERROR << "player enter game";
		return;
	}
	
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::OnLsLeaveGame(::google::protobuf::RpcController* controller,
    const ::ControllerNodeLsLeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 

	auto player_id = GetPlayerIdByConnId(request->session_id());
	ControllerPlayerSystem::LeaveGame(player_id);
	//todo statistics
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::OnLsDisconnect(::google::protobuf::RpcController* controller,
    const ::ControllerNodeLsDisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	auto player_id = GetPlayerIdByConnId(request->session_id());
	ControllerPlayerSystem::LeaveGame(player_id);
	controller_tls.gate_sessions().erase(player_id);
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::OnGsPlayerService(::google::protobuf::RpcController* controller,
    const ::NodeServiceMessageRequest* request,
    ::NodeServiceMessageResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	auto& message_extern = request->ex();
	auto it = controller_tls.player_list().find(message_extern.player_id());
	if (it == controller_tls.player_list().end())
	{
		LOG_ERROR << "player not found " << message_extern.player_id();
		return;
	}
	auto sit = g_service_method_info.find(request->msg().service_method_id());
	if (sit == g_service_method_info.end())
	{
		LOG_ERROR << "service_method_id not found " << request->msg().service_method_id();
		return;
	}
	auto service_it = g_player_services.find(sit->second.service);
	if (service_it == g_player_services.end())
	{
		LOG_ERROR << "player service  not found " << request->msg().service_method_id();
		return;
	}
	auto& serviceimpl = service_it->second;
	google::protobuf::Service* service = serviceimpl->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(sit->second.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found " << request->msg().service_method_id();
		//todo client error;
		return;
	}
	std::unique_ptr<google::protobuf::Message> player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(request->msg().body());
	std::unique_ptr<google::protobuf::Message> player_response(service->GetResponsePrototype(method).New());
	serviceimpl->CallMethod(method, it->second, get_pointer(player_request), get_pointer(player_response));
	if (nullptr == response)//不需要回复
	{
		return;
	}
	response->mutable_ex()->set_player_id(request->ex().player_id());
	response->mutable_msg()->set_body(player_response->SerializeAsString());
	response->mutable_msg()->set_service_method_id(request->msg().service_method_id());
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::AddCrossServerScene(::google::protobuf::RpcController* controller,
    const ::AddCrossServerSceneRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
    CreateSceneBySceneInfoP create_scene_param;
	for (auto& it : request->cross_scenes_info())
	{
		auto git = controller_tls.game_node().find(it.gs_node_id());
		if (git == controller_tls.game_node().end())
		{
			continue;
		}
		auto gs = git->second;
		auto try_gs_node_ptr = tls.registry.try_get<GsNodePtr>(gs);
		if (nullptr == try_gs_node_ptr)
		{
            LOG_ERROR << "gs not found ";
            continue;
		}
		create_scene_param.scene_info_ = it.scene_info();
        auto scene = ScenesSystem::CreateSceneByGuid(create_scene_param);
		tls.registry.emplace<GsNodePtr>(scene, *try_gs_node_ptr);
	}
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::EnterGsSucceed(::google::protobuf::RpcController* controller,
    const ::EnterGsSucceedRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	auto player = ControllerPlayerSystem::GetPlayer(request->player_id());
	if (entt::null == player)
	{
		return;
	}
	auto& player_session = tls.registry.get<PlayerSession>(player);
	auto gate_it = controller_tls.gate_nodes().find(player_session.gate_node_id());
	if (gate_it == controller_tls.gate_nodes().end())
	{
		LOG_ERROR << "gate crash" << player_session.gate_node_id();
		return;
	}
	
	auto game_it = controller_tls.game_node().find(request->game_node_id());
	if (game_it == controller_tls.game_node().end())
	{
        LOG_ERROR << "game crash" << request->game_node_id();
        return;
	}
	auto try_gs = tls.registry.try_get<GsNodePtr>(game_it->second);
	if (nullptr == try_gs)
	{
		LOG_ERROR << "game crash" << request->game_node_id();
        return;
	}
	player_session.set_gs(*try_gs);
	GateNodePlayerEnterGsRequest rq;
	rq.set_session_id(player_session.session_id());
	rq.set_gs_node_id(player_session.gs_node_id());
	gate_it->second->session_.CallMethod(GateServicePlayerEnterGs, &rq);
	PlayerChangeSceneSystem::SetChangeGsStatus(player, ControllerChangeSceneInfo::eEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
    const ::RouteMsgStringRequest* request,
    ::RouteMsgStringResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 

	if (request->route_data_list_size() >= kMaxRouteSize)
	{
		LOG_ERROR << "route msg size too max:" << request->DebugString();
		return;
	}
	else if (request->route_data_list_size() <= 0)
	{
		LOG_ERROR << "msg list empty:" << request->DebugString();
		return;
	}
	auto& route_data = request->route_data_list(request->route_data_list_size() - 1);
	auto sit = g_service_method_info.find(route_data.service_method_id());
	if (sit == g_service_method_info.end())
	{
		LOG_INFO << "service_method_id not found " << route_data.service_method_id();
		return;
	}
	const google::protobuf::MethodDescriptor* method = GetDescriptor()->FindMethodByName(sit->second.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found" << request->DebugString() << "method name" << route_data.method();
		return;
	}
	//当前节点的请求信息
	std::unique_ptr<google::protobuf::Message> current_node_request(GetRequestPrototype(method).New());
	if (!current_node_request->ParseFromString(request->body()))
	{
		LOG_ERROR << "invalid  body request" << request->DebugString() << "method name" << route_data.method();
		return;
	}
	//当前节点的真正回复的消息
	std::unique_ptr<google::protobuf::Message> current_node_response(GetResponsePrototype(method).New());
	CallMethod(method, NULL, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);
	auto mutable_request = const_cast<::RouteMsgStringRequest*>(request);
	//没有发送到下个节点就是要回复了
	if (cl_tls.next_route_node_type() == UINT32_MAX)
	{
		response->set_body(current_node_response->SerializeAsString());
		for (auto& it : request->route_data_list())
		{
			*response->add_route_data_list() = it;
		}
		response->set_session_id(request->session_id());
		return;
	}
	//处理,如果需要继续路由则拿到当前节点信息
	//需要发送到下个节点
	cl_tls.set_next_route_node_type(UINT32_MAX);
	auto next_route_data = mutable_request->add_route_data_list();
	next_route_data->CopyFrom(cl_tls.route_data());
	next_route_data->mutable_node_info()->CopyFrom(g_controller_node->node_info());
	mutable_request->set_body(cl_tls.route_msg_body());
	/*switch (cl_tls.next_route_node_type())
	{
	case kLoginNode:
	{
		g_controller_node->controller_node()->CallMethod(ControllerServiceRouteNodeStringMsg, mutable_request);
	}
	break;
	case kDatabaseNode:
	{
		g_controller_node->database_node()->CallMethod(DbServiceRouteNodeStringMsg, mutable_request);
	}
	break;
	case kGateNode:
	{
		g_controller_node->db_node()->CallMethod(DbServiceRouteNodeStringMsg, mutable_request);
	}
	break;
	case kGameNode:
	{
		g_controller_node->db_node()->CallMethod(DbServiceRouteNodeStringMsg, mutable_request);
	}
	break;
	default:
	{
		LOG_ERROR << "route to next node type error " << request->DebugString() << "," << cl_tls.next_route_node_type();
	}
	break;
	}*/
	cl_tls.set_next_route_node_id(UINT32_MAX);

///<<< END WRITING YOUR CODE 
}

void ControllerServiceImpl::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
    const ::RoutePlayerMsgStringRequest* request,
    ::RoutePlayerMsgStringResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
