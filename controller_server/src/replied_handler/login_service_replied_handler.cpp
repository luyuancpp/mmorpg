#include "login_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitLoginServiceLoginRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<LoginResponse>(std::bind(&OnLoginServiceLoginRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<LoginNodeCreatePlayerResponse>(std::bind(&OnLoginServiceCreatPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<LoginNodeEnterGameResponse>(std::bind(&OnLoginServiceEnterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnLoginServiceLeaveGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnLoginServiceDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnLoginServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnLoginServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnLoginServiceGateConnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnLoginServiceLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLoginServiceCreatPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginNodeCreatePlayerResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLoginServiceEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginNodeEnterGameResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLoginServiceLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLoginServiceDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLoginServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLoginServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLoginServiceGateConnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

