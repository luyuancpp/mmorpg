#include "database_service_replied_handler.h"

#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
void UpdateAccount(const ::account_database& a_d);
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
///<<< END WRITING YOUR CODE
}

void OnDbServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnDbServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

