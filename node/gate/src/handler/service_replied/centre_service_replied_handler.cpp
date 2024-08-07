#include "centre_service_replied_handler.h"
#include "network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitCentreServiceRegisterGameRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceRegisterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceRegisterGateRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceGatePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceGateSessionDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<LoginResponse>(std::bind(&OnCentreServiceLsLoginAccountRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceOnLoginEnterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceLsLeaveGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceLsDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeRouteMessageResponse>(std::bind(&OnCentreServiceGsPlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceEnterGsSucceedRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnCentreServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnCentreServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceUnRegisterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnCentreServiceRegisterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceRegisterGateRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceGatePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceGateSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceLsLoginAccountRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceOnLoginEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceLsLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceLsDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceGsPlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceEnterGsSucceedRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceUnRegisterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

