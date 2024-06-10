#include "centre_service_handler.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "mainscene_config.h"

#include "src/centre_node.h"
#include "src/system/scene/servernode_system.h"
#include "src/system/centre_player_system.h"
#include "src/comp/account_comp.h"
#include "src/comp/player_comp.h"
#include "src/constants/tips_id.h"
#include "src/system/scene/scene_system.h"
#include "src/thread_local/thread_local_storage_common_logic.h"
#include "src/network/game_node.h"
#include "src/network/login_node.h"
#include "src/network/rpc_msg_route.h"
#include "src/network/session.h"
#include "service/gate_service_service.h"
#include "service/game_service_service.h"
#include "service/service.h"
#include "src/handler/player_service.h"
#include "src/handler/register_handler.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_common_system.h"
#include "src/system/player_change_scene.h"
#include "src/thread_local/centre_thread_local_storage.h"
#include "src/util/defer.h"

#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_comp.pb.h"
#include "component_proto/player_network_comp.pb.h"

constexpr std::size_t kMaxPlayerSize{50000};

NodeId centre_node_id();

Guid GetPlayerIdBySessionId(const uint64_t session_id)
{
    auto eid = entt::to_entity(session_id);
    if (!tls.session_registry.valid(eid))
    {
		LOG_ERROR << "find session id" << session_id;
        return kInvalidGuid;
    }
    auto player_session_info = 
		tls.session_registry.try_get<PlayerSessionInfo>(eid);
	if (nullptr == player_session_info)
	{
        return kInvalidGuid;
	}
	return  player_session_info->player_id();
}

entt::entity GetPlayerByConnId(uint64_t session_id)
{
	return entt::entity{ GetPlayerIdBySessionId(session_id) };
}

///<<< END WRITING YOUR CODE
void CentreServiceHandler::StartGs(::google::protobuf::RpcController* controller,
	const ::CtrlStartGsRequest* request,
	::CtrlStartGsResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	response->set_centre_node_id(centre_node_id());
	const InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	const InetAddress service_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity game_node{entt::null};
	for (auto e : tls.network_registry.view<RpcServerConnection>())
	{
		if (tls.network_registry.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != 
			session_addr.toIpPort())
		{
			continue;
		}
		game_node = e;
		break;
	}
	if (game_node == entt::null)
	{
		//todo
		LOG_INFO << "game connection not found " << request->gs_node_id();
		return;
	}
	entt::entity game_node_id{ request->gs_node_id() };

	auto game_node1 = tls.game_node_registry.create(game_node_id);

	auto c = tls.registry.get<RpcServerConnection>(game_node);
	auto game_node_ptr = std::make_shared<GameNodeClient::element_type>(c.conn_);
	game_node_ptr->node_info_.set_node_id(request->gs_node_id());
	game_node_ptr->node_info_.set_node_type(kGameNode);
	game_node_ptr->node_inet_addr_ = service_addr; //为了停掉gs，或者gs断线用
	game_node_ptr->server_entity_ = game_node;
	AddMainSceneNodeComponent(game_node);
	tls.game_node_registry.emplace<GameNodeClient>(game_node1, game_node_ptr);
	if (request->server_type() == kMainSceneNode)
	{
        tls.game_node_registry.emplace<MainSceneServer>(game_node);
	}
	else if (request->server_type() == kMainSceneCrossNode)
	{
		tls.game_node_registry.emplace<CrossMainSceneServer>(game_node);
	}
	else if (request->server_type() == kRoomSceneCrossNode)
	{
		tls.game_node_registry.emplace<CrossRoomSceneServer>(game_node);
	}
	else
	{
		tls.game_node_registry.emplace<RoomSceneServer>(game_node);
	}

	for (auto gate : tls.registry.view<GateNodeClient>())
	{
		g_centre_node->LetGateConnect2Gs(game_node, gate);
	}
	LOG_DEBUG << "gs connect node id: " << request->gs_node_id() << response->DebugString() << "server type:" << request->server_type();
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::RegisterGate(::google::protobuf::RpcController* controller,
	const ::RegisterGateRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	entt::entity gate{ request->gate_node_id() };
	for (auto e : tls.network_registry.view<RpcServerConnection>())
	{
		auto c = tls.network_registry.get<RpcServerConnection>(e);
		if (c.conn_->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		auto gate_node_id = tls.gate_node_registry.create(gate);
		if (gate_node_id != gate)
		{
			LOG_ERROR << "create gate error";
			return;
		}
		auto& gate_node = tls.gate_node_registry.emplace<GateNodeClient>(gate, 
			std::make_shared<GateNodeClient::element_type>(c.conn_));
		gate_node->node_info_.set_node_id(request->gate_node_id());
		gate_node->node_info_.set_node_type(kGateNode);
		break;
	}
	if (entt::null == gate)
	{
		return;
	}
	for (auto e : tls.registry.view<GameNodeClient>())
	{
		g_centre_node->LetGateConnect2Gs(e, gate);
	}
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::GatePlayerService(::google::protobuf::RpcController* controller,
	const ::GateClientMessageRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::GateDisconnect(::google::protobuf::RpcController* controller,
	const ::GateDisconnectRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	auto sid = entt::to_entity(cl_tls.session_id());
	if (!tls.session_registry.valid(sid))
	{
		LOG_DEBUG << "can not find " << cl_tls.session_id();
		return;
	}
    defer(Destroy(tls.session_registry, sid));
	//断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
	//考虑a 断 b 断 a 断 b 断.....(中间不断重连)
	const auto player = GetPlayerByConnId(request->session_id());
	if (entt::null == player)
	{
		return;
	}
	const auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	//玩家已经断开连接了
	if (nullptr == player_node_info)
	{
		return;
	}
	//notice 异步过程 gate 先重连过来，然后断开才收到，也就是会把新来的连接又断了，极端情况，也要测试如果这段代码过去了，会有什么问题
	if (player_node_info->gate_session_id() != request->session_id())
	{
		return;
	}
	entt::entity game_node_id{ player_node_info->game_node_id() };
    if (!tls.game_node_registry.valid(game_node_id))
    {
        LOG_ERROR << "gs not found ";
        return;
    }
    auto game_node = tls.registry.try_get<GameNodeClient>(game_node_id);
    if (nullptr == game_node)
    {
        LOG_ERROR << "gs not found ";
		return;
    }
	const auto player_id = tls.registry.get<Guid>(player);
	GameNodeDisconnectRequest rq;
	rq.set_player_id(player_id);
	(*game_node)->session_.CallMethod(GameServiceDisconnectMsgId, rq);
	CenterPlayerSystem::LeaveGame(player_id);
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::LsLoginAccount(::google::protobuf::RpcController* controller,
	const ::LoginRequest* request,
	::LoginResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    
	if (cl_tls.player_list().size() >= kMaxPlayerSize)
	{
		//如果登录的是新账号,满了得去排队,是账号排队，还是角色排队>???
		response->mutable_error()->set_id(kRetLoginAccountPlayerFull);
		return;
	}
	//排队
    //todo 排队队列里面有同一个人的两个链接
	//如果不是同一个登录服务器,踢掉已经登录的账号
	//告诉客户端登录中
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::LsEnterGame(::google::protobuf::RpcController* controller,
	const ::EnterGameRequest* request,
	::EnterGameResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

	//顶号
	//todo正常或者顶号进入场景
	//todo 断线重连进入场景，断线重连分时间
	//todo 返回login session 删除了后能返回客户端吗?数据流程对吗
	auto session = tls.session_registry.create(entt::entity(cl_tls.session_id()));
	if (session != entt::entity(cl_tls.session_id()))
	{
		LOG_ERROR << "session not equal " << cl_tls.session_id() << " " << entt::to_integral(session);
	}
	tls.session_registry.emplace<PlayerSessionInfo>(session).set_player_id(request->player_id());

     auto player_it = cl_tls.player_list().find(request->player_id());
    if (player_it == cl_tls.player_list().end())
	{
		//把旧的connection 断掉
		const auto player = tls.registry.create();
		auto ret = cl_tls.player_list().emplace(request->player_id(), player);
		if (!ret.second)
		{
			LOG_ERROR << "login create player error" << request->player_id();
			return;
		}

		tls.registry.emplace<PlayerAccount>(player, std::make_shared<PlayerAccount::element_type>());
		tls.registry.emplace_or_replace<PlayerNodeInfo>(player).set_gate_session_id(cl_tls.session_id());

		PlayerCommonSystem::InitPlayerComponent(player, request->player_id());

		GetSceneParam get_scene_param;
		get_scene_param.scene_conf_id_ = 1;
		const auto scene = NodeSceneSystem::GetNotFullScene(get_scene_param);
		//找不到上次的场景，放到默认场景里面
		if (scene == entt::null)
		{
			// todo default
			LOG_INFO << "player " << request->player_id() << " enter default secne";
		}
		//第一次进入
		tls.registry.emplace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_FIRST);

		//todo 会话没有了玩家还在

		PlayerSceneSystem::CallPlayerEnterGs(player, PlayerSceneSystem::GetGameNodeIdByScene(scene), cl_tls.session_id());
		ControllerChangeSceneInfo change_scene_info;
		change_scene_info.mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfo>(scene));
		change_scene_info.set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);
		change_scene_info.set_change_gs_status(ControllerChangeSceneInfo::eEnterGsSceneSucceed);
		PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_scene_info);
	}
	else
	{
		auto player = player_it->second;
		//顶号,断线重连 记得gate 删除 踢掉老gate,但是是同一个gate就不用了
		//顶号的时候已经在场景里面了,不用再进入场景了
		//todo换场景的过程中被顶了
		//告诉账号被顶
		//断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
		//区分顶号和断线重连
		if (auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
			nullptr != player_node_info)
		{
			extern const uint32_t ClientPlayerCommonServiceBeKickMsgId;
			TipS2C beKickTip;
			beKickTip.mutable_tips()->set_id(kRetLoginBeKickByAnOtherAccount);
			Send2Player(ClientPlayerCommonServiceBeKickMsgId, beKickTip, request->player_id());
			//删除老会话,需要玩家收到消息后再删除gate连接
			Destroy(tls.session_registry, entt::entity{ player_node_info->gate_session_id() });
			GateNodeKickConnRequest message;
			message.set_session_id(cl_tls.session_id());
			Send2Gate(GateServiceKickConnByCentreMsgId, message, 
				get_gate_node_id(player_node_info->gate_session_id()));

			player_node_info->set_gate_session_id(cl_tls.session_id());
		}
		else
		{
			tls.registry.emplace_or_replace<PlayerNodeInfo>(player).set_gate_session_id(cl_tls.session_id());
		}
		//连续顶几次,所以用emplace_or_replace
		tls.registry.emplace_or_replace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_REPLACE);
	}
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::LsLeaveGame(::google::protobuf::RpcController* controller,
	const ::CtrlLsLeaveGameRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	CenterPlayerSystem::LeaveGame(GetPlayerIdBySessionId(cl_tls.session_id()));
	//todo statistics
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::LsDisconnect(::google::protobuf::RpcController* controller,
	const ::CtrlLsDisconnectRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	defer(Destroy(tls.registry, entt::entity{ cl_tls.session_id() }));
	const auto player_id = GetPlayerIdBySessionId(cl_tls.session_id());
	CenterPlayerSystem::LeaveGame(player_id);
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::GsPlayerService(::google::protobuf::RpcController* controller,
	const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	auto sid = entt::to_entity(request->ex().session_id());
	if (!tls.session_registry.valid(sid))
	{
		return;
	}
    const auto player_info =
	tls.session_registry.try_get<PlayerSessionInfo>(sid);
    if (nullptr == player_info)
    {
        LOG_ERROR << "session not found " << request->ex().session_id();
        return;
    }
    auto player = cl_tls.get_player(player_info->player_id());
    if (tls.registry.valid(player))
    {
        LOG_ERROR << "player not found " << player_info->player_id();
        return;
    }
	if (request->msg().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << request->msg().message_id();
		return;
	}
	const auto& message_info = g_message_info.at(request->msg().message_id());
	const auto service_it = g_player_service.find(message_info.service);
	if (service_it == g_player_service.end())
	{
		LOG_ERROR << "player service  not found " << request->msg().message_id();
		return;
	}
	const auto& service_handler = service_it->second;
	google::protobuf::Service* service = service_handler->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found " << request->msg().message_id();
		//todo client error;
		return;
	}
	const MessagePtr player_request(service->GetRequestPrototype(method).New());
	if (!player_request->ParsePartialFromArray(request->msg().body().data(), int32_t(request->msg().body().size())))
	{
        LOG_ERROR << "ParsePartialFromArray " << request->msg().message_id();
        //todo client error;
        return;
	}
	const MessagePtr player_response(service->GetResponsePrototype(method).New());
	service_handler->CallMethod(method, player, get_pointer(player_request), get_pointer(player_response));
	if (nullptr == response)//不需要回复
	{
		return;
	}
	response->mutable_ex()->set_session_id(request->ex().session_id());
    auto byte_size = int32_t(response->ByteSizeLong());
	response->mutable_msg()->mutable_body()->resize(byte_size);
    // FIXME: error check
    if (response->SerializePartialToArray(response->mutable_msg()->mutable_body()->data(), byte_size))
    {
        LOG_ERROR << "message error " << this;
        return;
    }
	response->mutable_msg()->set_message_id(request->msg().message_id());
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::AddCrossServerScene(::google::protobuf::RpcController* controller,
	const ::AddCrossServerSceneRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	CreateGameNodeSceneParam create_scene_param;
	for (auto& it : request->cross_scenes_info())
	{
		entt::entity game_node_id{it.gs_node_id()};
		if (!tls.game_node_registry.valid(game_node_id))
		{
			continue;
		}
		auto game_node = tls.registry.try_get<GameNodeClient>(game_node_id);
		if (nullptr == game_node)
		{
            LOG_ERROR << "gs not found ";
            continue;
		}
		create_scene_param.scene_info = it.scene_info();
		auto scene = ScenesSystem::CreateScene2GameNode(create_scene_param);
		tls.registry.emplace<GameNodeClient>(scene, *game_node);
	}
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::EnterGsSucceed(::google::protobuf::RpcController* controller,
	const ::EnterGameNodeSucceedRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    auto player = cl_tls.get_player(request->player_id());
    if (!tls.registry.valid(player))
    {
        LOG_ERROR << "player not found " << request->player_id();
        return;
    }
	auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		LOG_ERROR << "player session not found" << request->player_id();
		return;
	}
	entt::entity gate_node_id{ get_gate_node_id(player_node_info->gate_session_id()) };
	if (tls.gate_node_registry.valid(gate_node_id))
	{
		LOG_ERROR << "gate crash" << get_gate_node_id(player_node_info->gate_session_id());
		return;
	}
	auto gate_node = tls.gate_node_registry.try_get<GateNodeClient>(gate_node_id);
	if (nullptr == gate_node)
	{
        LOG_ERROR << "gate crash" << get_gate_node_id(player_node_info->gate_session_id());
        return;
	}
	entt::entity game_node_id{ request->game_node_id() };
	if (!tls.game_node_registry.valid(game_node_id))
	{
        LOG_ERROR << "game crash" << request->game_node_id();
        return;
	}
	if (const auto* const game_node = tls.game_node_registry.try_get<GameNodeClient>(game_node_id);
		nullptr == game_node)
	{
		LOG_ERROR << "game crash" << request->game_node_id();
		return;
	}
	player_node_info->set_game_node_id(request->game_node_id());
	GateNodePlayerUpdateGameNodeRequest rq;
	rq.set_session_id(player_node_info->gate_session_id());
	rq.set_game_node_id(player_node_info->game_node_id());
	(*gate_node)->session_.CallMethod(GateServicePlayerEnterGsMsgId, rq);
	PlayerChangeSceneSystem::SetChangeGsStatus(player, 
		ControllerChangeSceneInfo::eEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
	const ::RouteMsgStringRequest* request,
	::RouteMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

    //函数返回前一定会执行的函数
    defer(cl_tls.set_next_route_node_type(UINT32_MAX));
    defer(cl_tls.set_next_route_node_id(UINT32_MAX));
	defer(cl_tls.set_current_session_id(kInvalidSessionId));

    cl_tls.set_current_session_id(request->session_id());

if (request->route_data_list_size() >= kMaxRouteSize)
{
	LOG_ERROR << "route msg size too max:" << request->DebugString();
	return;
}
if (request->route_data_list().empty())
{
	LOG_ERROR << "msg list empty:" << request->DebugString();
	return;
}
auto& route_data = request->route_data_list(request->route_data_list_size() - 1);
if (route_data.message_id() >= g_message_info.size())
{
	LOG_INFO << "message_id not found " << route_data.message_id();
	return;
}
const auto& message_info = g_message_info[route_data.message_id()];
if (nullptr == message_info.service_impl_instance_)
{
	LOG_INFO << "message_id not found " << route_data.message_id();
	return;
}

const auto it = g_server_service.find(message_info.service);
if (it == g_server_service.end())
{
	LOG_INFO << "message_id not found " << route_data.message_id();
	return;
}
const auto& servcie = it->second;
const google::protobuf::MethodDescriptor* method = servcie->GetDescriptor()->FindMethodByName(message_info.method);
if (nullptr == method)
{
	LOG_ERROR << "method not found" << request->DebugString();
	return;
}
//当前节点的请求信息
std::unique_ptr<google::protobuf::Message> current_node_request(GetRequestPrototype(method).New());
if (!current_node_request->ParsePartialFromArray(request->body().data(), int32_t(request->body().size())))
{
	LOG_ERROR << "invalid  body request" << request->DebugString();
	return;
}

//当前节点的真正回复的消息
std::unique_ptr<google::protobuf::Message> current_node_response(GetResponsePrototype(method).New());
servcie->CallMethod(method, NULL, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);

auto* mutable_request = const_cast<::RouteMsgStringRequest*>(request);
//没有发送到下个节点就是要回复了
if (cl_tls.next_route_node_type() == UINT32_MAX)
{
	auto byte_size = int32_t(current_node_response->ByteSizeLong());
	response->mutable_body()->resize(byte_size);
	current_node_response->SerializePartialToArray(response->mutable_body()->data(), byte_size);
	for (auto& request_data_it : request->route_data_list())
	{
		*response->add_route_data_list() = request_data_it;
	}
	response->set_session_id(cl_tls.session_id());
	response->set_id(request->id());
	return;
}
//处理,如果需要继续路由则拿到当前节点信息
//需要发送到下个节点
const auto next_route_data = mutable_request->add_route_data_list();
next_route_data->CopyFrom(cl_tls.route_data());
next_route_data->mutable_node_info()->CopyFrom(g_centre_node->node_info());
mutable_request->set_body(cl_tls.route_msg_body());
    mutable_request->set_id(request->id());

    switch (cl_tls.next_route_node_type())
    {
    case kGateNode:
	{
		entt::entity gate_node_id{ cl_tls.next_route_node_id() };
        if (tls.gate_node_registry.valid(gate_node_id))
        {
            LOG_ERROR << "gate crash " << cl_tls.next_route_node_id();
            return;
        }
        auto gate_node = tls.gate_node_registry.try_get<GateNodeClient>(gate_node_id);
        if (nullptr == gate_node)
        {
            LOG_ERROR << "gate crash " << cl_tls.next_route_node_id();
            return;
        }
		(*gate_node)->session_.Route2Node(GateServiceRouteNodeStringMsgMsgId, *mutable_request);
	}
	break;
    case kGameNode:
	{
		entt::entity game_node_id{ cl_tls.next_route_node_id() };
		if (!tls.game_node_registry.valid(game_node_id))
		{
            LOG_ERROR << "game not found game " << cl_tls.next_route_node_id() << request->DebugString();
            return;
		}
		auto game_node = tls.registry.try_get<GameNodeClient>(game_node_id);
		if (nullptr == game_node)
		{
			LOG_ERROR << "game not found game " << cl_tls.next_route_node_id() << request->DebugString();
			return;
		}
		(*game_node)->session_.Route2Node(GameServiceRouteNodeStringMsgMsgId, *mutable_request);
	}
	    break;
    default:
	    {
		    LOG_ERROR << "route to next node type error " << request->DebugString() << "," << cl_tls.next_route_node_type();
	    }
	break;
}

///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
	const ::RoutePlayerMsgStringRequest* request,
	::RoutePlayerMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

