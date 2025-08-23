#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "network/codec/dispatcher.h"
#include "service_info/service_info.h"
#include "thread_local/redis_manager.h"
#include "util/defer.h"
#include "session/manager/session_manager.h"
#include "thread_local/message_context.h"
#include "service_info/gate_service_service_info.h"

extern ProtobufDispatcher gResponseDispatcher;

void OnServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMessageResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	defer(tlsMessageContext.SetNextRouteNodeType(UINT32_MAX));
	defer(tlsMessageContext.SeNextRouteNodeId(UINT32_MAX));
	defer(tlsMessageContext.SetCurrentSessionId(kInvalidSessionId));

	tlsMessageContext.SetCurrentSessionId(replied->session_id());

	if (replied->route_nodes_size() <= 0)
	{
		LOG_ERROR << "msg list empty:" << replied->DebugString();
		return;
	}
	
	auto& route_data = replied->route_nodes(replied->route_nodes_size() - 1);
	if ( route_data.message_id() >= gRpcServiceRegistry.size())
	{
		LOG_ERROR << "message_id not found " << route_data.message_id();
		return;
	}
	//gate 和其他服务器不一样，直接返回消息给客户端
	const auto it = tlsSessionManager.sessions().find(replied->session_id());
	if (it == tlsSessionManager.sessions().end())
	{
		LOG_ERROR << "conn id not found  session id "  << "," << replied->session_id();
		return;
	}

	MessageContent message;
	message.set_serialized_message(replied->body());
	message.set_message_id(route_data.message_id());;
	gGateNode->SendMessageToClient(it->second.conn, message);
	///<<< END WRITING YOUR CODE
}

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}
