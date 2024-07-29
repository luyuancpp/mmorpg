#include "muduo/base/Logging.h"

#include "thread_local/storage_common_logic.h"
#include "gate_node.h"
#include "network/codec/dispatcher.h"
#include "thread_local/storage_gate.h"
#include "util/defer.h"
#include "service/service.h"
#include "network/gate_session.h"

#include "service/gate_service_service.h"

extern ProtobufDispatcher g_response_dispatcher;

void OnServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	defer(tlsCommonLogic.set_next_route_node_type(UINT32_MAX));
	defer(tlsCommonLogic.set_next_route_node_id(UINT32_MAX));
	defer(tlsCommonLogic.set_current_session_id(kInvalidSessionId));

    tlsCommonLogic.set_current_session_id(replied->session_id());

	if (replied->route_data_list_size() <= 0)
	{
		LOG_ERROR << "msg list empty:" << replied->DebugString();
		return;
	}
	
	auto& route_data = replied->route_data_list(replied->route_data_list_size() - 1);
	if ( route_data.message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << route_data.message_id();
		return;
	}
	//gate 和其他服务器不一样，直接返回消息给客户端
	const auto it = tls_gate.sessions().find(replied->session_id());
	if (it == tls_gate.sessions().end())
	{
		LOG_ERROR << "conn id not found  session id "  << "," << replied->session_id();
		return;
	}
	MessageBody message;
	message.set_body(replied->body());
	message.set_message_id(route_data.message_id());;
	g_gate_node->Send(it->second.conn_, message);
	///<<< END WRITING YOUR CODE
}

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}
