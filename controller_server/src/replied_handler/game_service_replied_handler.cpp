#include "game_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitGameServiceEnterGsRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<NodeServiceMessageResponse>(std::bind(&OnGameServiceSend2PlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<GameNodeRpcClientResponse>(std::bind(&OnGameServiceClientSend2PlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeServiceMessageResponse>(std::bind(&OnGameServiceCallPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGameServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGameServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGameServiceSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceClientSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GameNodeRpcClientResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceCallPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

