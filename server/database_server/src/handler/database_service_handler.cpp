#include "database_service_handler.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/util/defer.h"
#include "src/database_server.h"
#include "service/service.h"
///<<< END WRITING YOUR CODE
void DbServiceHandler::Login(::google::protobuf::RpcController* controller,
	const ::DatabaseNodeLoginRequest* request,
	::DatabaseNodeLoginResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	::account_database& account_db = *response->mutable_account_player();
	g_database_node->redis_client()->Load(account_db, request->account());
	if (response->ByteSizeLong() > 0)
	{
		g_database_node->player_mysql_client()->LoadOne(account_db,
			std::string("account = '") + request->account() + std::string("'"));
	}
	if (account_db.ByteSizeLong() > 0)
	{
		account_db.set_account(request->account());
		g_database_node->redis_client()->Save(account_db, request->account());
	}
///<<< END WRITING YOUR CODE
}

void DbServiceHandler::CreatePlayer(::google::protobuf::RpcController* controller,
	const ::DatabaseNodeCreatePlayerRequest* request,
	::DatabaseNodeCreatePlayerResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	::account_database& r_db = *response->mutable_account_player();
	g_database_node->redis_client()->Load(r_db, request->account());
	player_database new_player;
	new_player.set_register_time(muduo::Timestamp::now().microSecondsSinceEpoch());
	g_database_node->player_mysql_client()->SaveOne(new_player);
	response->set_player_id(g_database_node->player_mysql_client()->LastInsertId());
	new_player.set_player_id(response->player_id());
	r_db.mutable_simple_players()->add_players()->set_player_id(response->player_id());
	g_database_node->redis_client()->Save(new_player, new_player.player_id());
	g_database_node->redis_client()->Save(r_db, r_db.account());
///<<< END WRITING YOUR CODE
}

void DbServiceHandler::EnterGame(::google::protobuf::RpcController* controller,
	const ::DatabaseNodeEnterGameRequest* request,
	::DatabaseNodeEnterGameResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	player_database new_player;
	const std::string where_case = std::string("player_id = '") +
		std::to_string(request->player_id()) +
		std::string("'");
	g_database_node->player_mysql_client()->LoadOne(new_player, where_case);
	assert(new_player.player_id() > 0);
	//todo  optimize
	g_database_node->redis_client()->Save(new_player, new_player.player_id());
///<<< END WRITING YOUR CODE
}

void DbServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
	const ::RouteMsgStringRequest* request,
	::RouteMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	//todo mysql 速度不够快,应该换成消息队列去处理,防止卡死mysql
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
	const auto& receive_route_data = request->route_data_list(request->route_data_list_size() - 1);
	const auto& service_method_info = g_message_info.at(receive_route_data.message_id());
	const google::protobuf::MethodDescriptor* method = GetDescriptor()->FindMethodByName(service_method_info.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found" << request->DebugString();
		return;
	}
	//当前节点的请求信息
	const MessagePtr current_node_request(GetRequestPrototype(method).New());
	if (!current_node_request->ParsePartialFromArray(request->body().data(), int32_t(request->body().size())))
	{
		LOG_ERROR << "invalid  body request" << request->DebugString();
		return;
	}
	//当前节点的真正回复的消息
	const MessagePtr current_node_response(GetResponsePrototype(method).New());
	CallMethod(method, nullptr, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);
	response->set_body(current_node_response->SerializeAsString());
	for (const auto& data : request->route_data_list())
	{
		*response->add_route_data_list() = data;
	}
	response->set_session_id(cl_tls.session_id());
	response->set_id(request->id());
	response->set_is_client(request->is_client());
///<<< END WRITING YOUR CODE
}

void DbServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
	const ::RoutePlayerMsgStringRequest* request,
	::RoutePlayerMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

