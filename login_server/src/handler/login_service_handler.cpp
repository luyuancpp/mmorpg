#include "login_service_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE  
#include "muduo/base/Logging.h"

#include "src/comp/account_player.h"
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/game_logic/tips_id.h"
#include "src/login_server.h"
#include "src/network/gate_node.h"
#include "src/network/node_info.h"
#include "src/network/route_system.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_msg_route.h"
#include "src/network/server_component.h"
#include "src/pb/pbc/controller_service_service.h"
#include "src/pb/pbc/database_service_service.h"
#include "src/pb/pbc/service.h"
#include "src/redis_client/redis_client.h"
#include "src/thread_local/login_thread_local_storage.h"
#include "src/util/defer.h"

#include "controller_service.pb.h"

void EnterGame(Guid player_id)
{
	auto it = login_tls.session_list().find(cl_tls.session_id());
	if (login_tls.session_list().end() == it)
	{
		return;
	}
	CtrlEnterGameRequest enter_game_request;
	enter_game_request.set_player_id(player_id);
	Route2Node(kControllerNode, ControllerServiceLsEnterGameMsgId, enter_game_request);
	login_tls.session_list().erase(cl_tls.session_id());
}

///<<< END WRITING YOUR CODE
void LoginServiceHandler::Login(::google::protobuf::RpcController* controller,
	const ::LoginRequest* request,
	::LoginResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	//todo 测试用例连接不登录马上断线，
	//todo 账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//todo 登录的时候马上断开连接换了个gate应该可以登录成功
	//todo 在链接过程中断了，换了gate新的gate 应该是可以上线成功的，消息要发到新的gate上,老的gate正常走断开流程
	CtrlLoginAccountRequest ctrl_login_request;
	ctrl_login_request.set_account(request->account());
	login_tls.session_list().emplace(cl_tls.session_id(), std::make_shared<PlayerPtr::element_type>());
	Route2Node(kControllerNode, ControllerServiceLsLoginAccountMsgId, ctrl_login_request);
///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::CreatPlayer(::google::protobuf::RpcController* controller,
	const ::CreatePlayerC2lRequest* request,
	::LoginNodeCreatePlayerResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	// login process
	//check name rule
	const auto sit = login_tls.session_list().find(request->session_id());
	if (sit == login_tls.session_list().end())
	{
		ReturnClosureError(kRetLoginCreatePlayerConnectionHasNotAccount);
	}
	CheckReturnClosureError(sit->second->CreatePlayer());
	// database process
	DatabaseNodeCreatePlayerRequest db_create_request;
	db_create_request.set_account(sit->second->account());
	Route2Node(kDatabaseNode, DbServiceCreatePlayerMsgId, db_create_request);
}

void LoginServiceHandler::EnterGame(::google::protobuf::RpcController* controller,
	const ::EnterGameC2LRequest* request,
	::LoginNodeEnterGameResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	auto sit = login_tls.session_list().find(request->session_id());
	if (sit == login_tls.session_list().end())
	{
		ReturnClosureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	// check second times change player id error 
	CheckReturnClosureError(sit->second->EnterGame());

	// long time in login processing
	auto player_id = request->player_id();
	if (!sit->second->HasPlayer(player_id))
	{
		ReturnClosureError(kRetLoginPlayerGuidError);
	}
	//todo 已经在其他login
	player_database new_player;
	g_login_node->redis_client()->Load(new_player, player_id);
	//test
	sit->second->Playing(player_id);
	response->set_session_id(request->session_id());
	//test
	response->set_player_id(player_id);
	if (new_player.player_id() > 0)
	{
		//玩家数据已经在redis里面了直接进入游戏
		::EnterGame(player_id);
		return;
	}
	// redis没有玩家数据去数据库取
	DatabaseNodeEnterGameRequest database_enter_game_request;
	database_enter_game_request.set_player_id(player_id);
	Route2Node(kDatabaseNode, DbServiceEnterGameMsgId, database_enter_game_request);
}

void LoginServiceHandler::LeaveGame(::google::protobuf::RpcController* controller,
	const ::LeaveGameC2LRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	auto sit = login_tls.session_list().find(request->session_id());
	if (sit == login_tls.session_list().end())
	{
		LOG_ERROR << " leave game not found connection";
		return;
	}
	//连接过，登录过
	CtrlLsLeaveGameRequest rq;
	rq.set_session_id(request->session_id());
	g_login_node->controller_node()->Send(ControllerServiceLsLeaveGameMsgId, rq);
	login_tls.session_list().erase(sit);
	///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::Disconnect(::google::protobuf::RpcController* controller,
	const ::LoginNodeDisconnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
		//比如:登录还没到controller,gw的disconnect 先到，登录后到，那么controller server 永远删除不了这个sessionid了
	login_tls.session_list().erase(request->session_id());
	CtrlLsDisconnectRequest rq;
	rq.set_session_id(request->session_id());
	g_login_node->controller_node()->Send(ControllerServiceLsDisconnectMsgId, rq);
	///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
	const ::RouteMsgStringRequest* request,
	::RouteMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
    defer(cl_tls.set_next_route_node_type(UINT32_MAX));
    defer(cl_tls.set_next_route_node_id(UINT32_MAX));
    defer(cl_tls.set_current_session_id(kInvalidSessionId));

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
	//当前节点收到的数据
	const auto& route_data = request->route_data_list(request->route_data_list_size() - 1);
	if (route_data.message_id() >= g_message_info.size())
	{
		LOG_INFO << "message_id not found " << route_data.message_id();
		return;
	}
	const auto& message_info = g_message_info.at(route_data.message_id());
	const google::protobuf::MethodDescriptor* method = GetDescriptor()->FindMethodByName(message_info.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found" << request->DebugString();
		return;
	}
	//当前节点的请求信息
	const MessagePtr current_node_request(GetRequestPrototype(method).New());
	if (!current_node_request->ParseFromString(request->body()))
	{
		LOG_ERROR << "invalid  body request" << request->DebugString();
		return;
	}
	cl_tls.set_current_session_id(request->session_id());
	//当前节点的真正回复的消息
	const MessagePtr current_node_response(GetResponsePrototype(method).New());
	CallMethod(method, nullptr, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);
	auto* const mutable_request = const_cast<::RouteMsgStringRequest*>(request);
	//没有发送到下个节点就是要回复了
	if (cl_tls.next_route_node_type() == UINT32_MAX)
	{
		response->set_body(current_node_response->SerializeAsString());
		for (const auto& data : request->route_data_list())
		{
			*response->add_route_data_list() = data;
		}
		response->set_session_id(request->session_id());
		response->set_id(request->id());
		return;
	}
	//处理,如果需要继续路由则拿到当前节点信息
	//需要发送到下个节点
	auto* const send_route_data = mutable_request->add_route_data_list();
	send_route_data->CopyFrom(cl_tls.route_data());
	send_route_data->mutable_node_info()->CopyFrom(g_login_node->node_info());
	mutable_request->set_body(cl_tls.route_msg_body());
	mutable_request->set_id(request->id());
	switch (cl_tls.next_route_node_type())
	{
	case kControllerNode:
	{
		//发送到下个节点
		g_login_node->controller_node()->Route2Node(ControllerServiceRouteNodeStringMsgMsgId, *mutable_request);
	}
		break;
	case kDatabaseNode:
		{
			//发送到下个节点
			g_login_node->db_node()->Route2Node(DbServiceRouteNodeStringMsgMsgId, *mutable_request);
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

void LoginServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
	const ::RoutePlayerMsgStringRequest* request,
	::RoutePlayerMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::GateConnect(::google::protobuf::RpcController* controller,
	const ::GateConnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
 	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	entt::entity gate{ entt::null };
	for (auto e : tls.registry.view<RpcServerConnection>())
	{
		auto c = tls.registry.get<RpcServerConnection>(e);
		if (c.conn_->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		gate = e;
		auto& gate_node = tls.registry.emplace<GateNodePtr>(gate, std::make_shared<GateNodePtr::element_type>(c.conn_));
		gate_node->node_info_.set_node_id(request->gate_node_id());
		gate_node->node_info_.set_node_type(kGateNode);
		gate_node->entity_id_ = e;
		login_tls.gate_nodes().emplace(request->gate_node_id(), gate_node);
		break;
	}
	if (entt::null == gate)
	{
		return;
	}
	tls.registry.emplace<InetAddress>(gate, session_addr);

///<<< END WRITING YOUR CODE
}

