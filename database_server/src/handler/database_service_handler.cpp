#include "database_service_handler.h"
#include "src/network/rpc_msg_route.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/database_server.h"
#include "src/pb/pbc/service.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void DbServiceHandler::Login(::google::protobuf::RpcController* controller,
    const ::DatabaseNodeLoginRequest* request,
    ::DatabaseNodeLoginResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	::account_database& db = *response->mutable_account_player();
	auto& account = request->account();
	g_database_node->redis_client()->Load(db, account);
	if (response->account_player().password().empty())
	{
		g_database_node->player_mysql_client()->LoadOne(db,
			std::string("account = '") + account + std::string("'"));
	}
	if (!db.password().empty())
	{
		db.set_account(account);
		g_database_node->redis_client()->Save(db, account);
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
	std::string where_case = std::string("player_id = '") +
		std::to_string(request->player_id()) +
		std::string("'");
	g_database_node->player_mysql_client()->LoadOne(new_player, where_case);
	assert(new_player.player_id() > 0);
	g_database_node->redis_client()->Save(new_player, new_player.player_id());
///<<< END WRITING YOUR CODE
}

void DbServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
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
	auto& receive_route_data = request->route_data_list(request->route_data_list_size() - 1);
	auto& service_method_info = g_message_info[receive_route_data.message_id()];
	const google::protobuf::MethodDescriptor* method = GetDescriptor()->FindMethodByName(service_method_info.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found" << request->DebugString();
		return;
	}
	//当前节点的请求信息
	std::unique_ptr<google::protobuf::Message> current_node_request(GetRequestPrototype(method).New());
	if (!current_node_request->ParseFromString(request->body()))
	{
		LOG_ERROR << "invalid  body request" << request->DebugString();
		return;
	}
	//当前节点的真正回复的消息
	std::unique_ptr<google::protobuf::Message> current_node_response(GetResponsePrototype(method).New());
	CallMethod(method, NULL, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);
	auto mutable_request = const_cast<::RouteMsgStringRequest*>(request);
	response->set_body(current_node_response->SerializeAsString());
	for (auto& it : request->route_data_list())
	{
		*response->add_route_data_list() = it;
	}
	response->set_session_id(request->session_id());
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

///<<<rpc end
