#include "centre_service_handler.h"
#include "thread_local/thread_local_storage.h"
#include "network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "mainscene_config.h"

#include "centre_node.h"
#include "system/scene/node_scene_system.h"
#include "system/centre_player_system.h"
#include "comp/player_comp.h"
#include "constants/tips_id.h"
#include "system/scene/scene_system.h"
#include "thread_local/thread_local_storage_common_logic.h"
#include "network/game_node.h"
#include "network/rpc_msg_route.h"
#include "network/gate_session.h"
#include "service/gate_service_service.h"
#include "service/game_service_service.h"
#include "service/service.h"
#include "handler/player_service.h"
#include "handler/register_handler.h"
#include "system/player_common_system.h"
#include "system/player_change_scene.h"
#include "thread_local/centre_thread_local_storage.h"
#include "util/defer.h"
#include "network/gate_session.h"
#include "type_alias/player_redis.h"
#include "type_alias/player_loading.h"
#include "util/pb_util.h"

#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_comp.pb.h"
#include "component_proto/player_network_comp.pb.h"
#include "constants_proto/node.pb.h"
#include "common_proto/mysql_database_table.pb.h"

constexpr std::size_t kMaxPlayerSize{50000};

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
	auto it = cl_tls.player_list().find(GetPlayerIdBySessionId(session_id));
	if (it == cl_tls.player_list().end())
	{
		return entt::null;
	}
	return it->second;
}

///<<< END WRITING YOUR CODE
void CentreServiceHandler::RegisterGame(::google::protobuf::RpcController* controller,
	const ::RegisterGameRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	const InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	const InetAddress service_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity game_node_id{request->game_node_id()};
	for (auto e : tls.network_registry.view<RpcSession>())
	{
		if (tls.network_registry.get<RpcSession>(e).conn_->peerAddress().toIpPort() != 
			session_addr.toIpPort())
		{
			continue;
		}
        auto game_node = tls.game_node_registry.create(game_node_id);
        if (game_node != game_node_id)
        {
            //todo
            LOG_INFO << "game connection not found " << request->game_node_id();
            return;
        }
        auto c = tls.network_registry.get<RpcSession>(e);

        auto game_node_ptr = std::make_shared<RpcSessionPtr::element_type>(c.conn_);
        AddMainSceneNodeComponent(tls.game_node_registry, game_node);
        tls.game_node_registry.emplace<RpcSessionPtr>(game_node, game_node_ptr);
        tls.game_node_registry.emplace<InetAddress>(game_node, service_addr);
		break;
	}
	
    LOG_INFO << " game register: " << MessageToJsonString(request);

	if (request->server_type() == eGameNodeType::kMainSceneCrossNode)
	{
		tls.game_node_registry.remove<MainSceneServer>(game_node_id);
		tls.game_node_registry.emplace<CrossMainSceneServer>(game_node_id);
	}
	else if (request->server_type() == eGameNodeType::kRoomNode)
	{
        tls.game_node_registry.remove<MainSceneServer>(game_node_id);
		tls.game_node_registry.emplace<RoomSceneServer>(game_node_id);
	}
    else if (request->server_type() == eGameNodeType::kRoomSceneCrossNode)
    {
        tls.game_node_registry.remove<MainSceneServer>(game_node_id);
        tls.game_node_registry.emplace<CrossRoomSceneServer>(game_node_id);
    }

	for (auto gate : tls.gate_node_registry.view<RpcSessionPtr>())
	{
		g_centre_node->BroadCastRegisterGameToGate(game_node_id, gate);
	}
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
	for (auto e : tls.network_registry.view<RpcSession>())
	{
		auto c = tls.network_registry.get<RpcSession>(e);
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
		auto& gate_node = tls.gate_node_registry.emplace<RpcSessionPtr>(gate, 
			std::make_shared<RpcSessionPtr::element_type>(c.conn_));
		break;
	}
	LOG_INFO << "gate register " << MessageToJsonString(request);
	for (auto e : tls.game_node_registry.view<RpcSessionPtr>())
	{
		g_centre_node->BroadCastRegisterGameToGate(e, gate);
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

void CentreServiceHandler::GateSessionDisconnect(::google::protobuf::RpcController* controller,
	const ::GateSessionDisconnectRequest* request,
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
    auto game_node = tls.game_node_registry.try_get<RpcSessionPtr>(game_node_id);
    if (nullptr == game_node)
    {
        LOG_ERROR << "gs not found ";
		return;
    }
	const auto player_id = tls.registry.get<Guid>(player);
	GameNodeDisconnectRequest rq;
	rq.set_player_id(player_id);
	(*game_node)->CallMethod(GameServiceDisconnectMsgId, rq);
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

void CentreServiceHandler::OnLoginEnterGame(::google::protobuf::RpcController* controller,
	const ::EnterGameL2Ctr* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

	//顶号
	//todo正常或者顶号进入场景
	//todo 断线重连进入场景，断线重连分时间
	//todo 返回login session 删除了后能返回客户端吗?数据流程对吗
	auto& rq = request->client_msg_body();
	auto session_uid = request->session_info().session_id();
	entt::entity session_id{ session_uid };
	auto session = tls.session_registry.create(session_id );
	if (session != session_id)
	{
		LOG_ERROR << "session not equal " << session_uid << " " << entt::to_integral(session);
	}
	tls.session_registry.emplace<PlayerSessionInfo>(session).set_player_id(rq.player_id());
    //todo把旧的connection 断掉
  
    auto player_it = cl_tls.player_list().find(rq.player_id());
    if (player_it == cl_tls.player_list().end())
	{
        tls.global_registry.get<PlayerLoadingInfoList>(global_entity()).emplace(
            rq.player_id(),
            std::move(*request));
        tls.global_registry.get<PlayerRedisPtr>(global_entity())->AsyncLoad(rq.player_id());
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
			Send2Player(ClientPlayerCommonServiceBeKickMsgId, beKickTip, rq.player_id());
			//删除老会话,需要玩家收到消息后再删除gate连接
			Destroy(tls.session_registry, entt::entity{ player_node_info->gate_session_id() });
			GateNodeKickConnRequest message;
			message.set_session_id(session_uid);
			Send2Gate(GateServiceKickConnByCentreMsgId, message, 
				get_gate_node_id(player_node_info->gate_session_id()));
			player_node_info->set_gate_session_id(session_uid);
		}
		else
		{
			tls.registry.emplace_or_replace<PlayerNodeInfo>(player).set_gate_session_id(
				session_uid);
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
	if (!tls.gate_node_registry.valid(gate_node_id))
	{
		LOG_ERROR << "gate crash" << get_gate_node_id(player_node_info->gate_session_id());
		return;
	}
	auto gate_node = tls.gate_node_registry.try_get<RpcSessionPtr>(gate_node_id);
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
	player_node_info->set_game_node_id(request->game_node_id());
	RegisterSessionGameNodeRequest rq;
	rq.mutable_session_info()->set_session_id(player_node_info->gate_session_id());
	rq.set_game_node_id(player_node_info->game_node_id());
	(*gate_node)->CallMethod(GateServicePlayerEnterGsMsgId, rq);
	PlayerChangeSceneSystem::SetChangeGsStatus(player, 
		CentreChangeSceneInfo::eEnterGsSceneSucceed);
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
        auto gate_node = tls.gate_node_registry.try_get<RpcSessionPtr>(gate_node_id);
        if (nullptr == gate_node)
        {
            LOG_ERROR << "gate crash " << cl_tls.next_route_node_id();
            return;
        }
		(*gate_node)->Route2Node(GateServiceRouteNodeStringMsgMsgId, *mutable_request);
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
		auto game_node = tls.game_node_registry.try_get<RpcSessionPtr>(game_node_id);
		if (nullptr == game_node)
		{
			LOG_ERROR << "game not found game " << cl_tls.next_route_node_id() << request->DebugString();
			return;
		}
		(*game_node)->Route2Node(GameServiceRouteNodeStringMsgMsgId, *mutable_request);
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

void CentreServiceHandler::UnRegisterGame(::google::protobuf::RpcController* controller,
	const ::UnRegisterGameRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    for (auto gate : tls.gate_node_registry.view<RpcSessionPtr>())
    {
        auto gate_node = tls.gate_node_registry.try_get<RpcSessionPtr>(gate);
        if (nullptr == gate_node)
        {
            LOG_ERROR << "gate not found ";
            continue;
        }
		UnRegisterGameRequest message;
		message.set_game_node_id(request->game_node_id());
        (*gate_node)->Send(GateServiceRegisterGameMsgId, message);
    }
///<<< END WRITING YOUR CODE
}

