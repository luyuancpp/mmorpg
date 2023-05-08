#include "gate_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitGateServiceStartGSHandler()
{
	g_response_dispatcher.registerMessageCallback<GateNodePlayerEnterGsResponese>(std::bind(&OnGateServicePlayerEnterGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGateServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGateServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGateServicePlayerEnterGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GateNodePlayerEnterGsResponese>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

