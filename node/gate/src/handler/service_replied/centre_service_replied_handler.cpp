#include "centre_service_replied_handler.h"
#include "network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitCentreServiceRegisterGameNodeRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceRegisterGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceRegisterGateNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceGatePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceGateSessionDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<LoginResponse>(std::bind(&OnCentreServiceLoginNodeAccountLoginRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceLoginNodeEnterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceLoginNodeLeaveGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceLoginNodeSessionDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeRouteMessageResponse>(std::bind(&OnCentreServicePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceEnterGsSucceedRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnCentreServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnCentreServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnCentreServiceUnRegisterGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnCentreServiceRegisterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceRegisterGateNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
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

void OnCentreServiceLoginNodeAccountLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceLoginNodeEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceLoginNodeLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServiceLoginNodeSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreServicePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeRouteMessageResponse>& replied, Timestamp timestamp)
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

void OnCentreServiceUnRegisterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

