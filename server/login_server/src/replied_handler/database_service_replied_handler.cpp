#include "database_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE

#include "src/thread_local/thread_local_storage_common_logic.h"
#include "src/network/route_system.h"
#include "src/thread_local/login_thread_local_storage.h"
#include "common_proto/c2gate.pb.h"

void UpdateAccount(const ::account_database& a_d)
{
	const auto session_it = login_tls.session_list().find(cl_tls.session_id());
	//断线了
	if (session_it == login_tls.session_list().end())
	{
		return;
	}
	session_it->second->set_account_data(a_d);
	session_it->second->OnDbLoaded();
}

void SendCtrlEnterGame(Guid player_id);
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitDbServiceLoginRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<DatabaseNodeLoginResponse>(std::bind(&OnDbServiceLoginRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<DatabaseNodeCreatePlayerResponse>(std::bind(&OnDbServiceCreatePlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<DatabaseNodeEnterGameResponse>(std::bind(&OnDbServiceEnterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnDbServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnDbServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnDbServiceLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<DatabaseNodeLoginResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	UpdateAccount(replied->account_player());
	LoginResponse response;
	for (const auto& it : replied->account_player().simple_players().players())
	{
		auto* const c_player = response.mutable_players()->Add();
		c_player->set_player_id(it.player_id());
	}
	SendPrevNodeResponse(response);
///<<< END WRITING YOUR CODE
}

void OnDbServiceCreatePlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<DatabaseNodeCreatePlayerResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	UpdateAccount(replied->account_player());
///<<< END WRITING YOUR CODE
}

void OnDbServiceEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<DatabaseNodeEnterGameResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	//db 加载过程中断线了
	//todo 只连接不登录,占用连接
	// login process
	// check account rule: empty , errno
	// check string rule
	if (const auto session_it = login_tls.session_list().find(cl_tls.session_id()); session_it == login_tls.session_list().end())
	{
		return;
	}
	SendCtrlEnterGame(replied->player_id());
///<<< END WRITING YOUR CODE
}

void OnDbServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	void OnServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr & conn, const std::shared_ptr<RouteMsgStringResponse>&replied, Timestamp timestamp);
	OnServiceRouteNodeStringMsgRepliedHandler(conn, replied, timestamp);
///<<< END WRITING YOUR CODE
}

void OnDbServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

