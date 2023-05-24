#include "muduo/base/Logging.h"

#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/game_logic/tips_id.h"
#include "src/gate_server.h"
#include "src/network/codec/dispatcher.h"
#include "src/thread_local/gate_thread_local_storage.h"
#include "src/util/defer.h"
#include "src/pb/pbc/service.h"

#include "gate_service_service.h"

extern ProtobufDispatcher g_response_dispatcher;

void OnServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE

	//函数返回前一定会执行的函数
	defer(cl_tls.set_next_route_node_type(UINT32_MAX));
	defer(cl_tls.set_next_route_node_id(UINT32_MAX));
	defer(cl_tls.set_current_session_id(kInvalidSessionId));

    cl_tls.set_current_session_id(replied->session_id());

	if (replied->route_data_list_size() <= 0)
	{
		LOG_ERROR << "msg list empty:" << replied->DebugString();
		return;
	}
	
	auto& route_data = replied->route_data_list(replied->route_data_list_size() - 1);
	if ( route_data.message_id() >= g_message_info.size())
	{
		LOG_INFO << "message_id not found " << route_data.message_id();
		return;
	}
	auto& message_info = g_message_info[route_data.message_id()];
	const google::protobuf::MethodDescriptor* method = g_gate_node->gate_service_hanlder().GetDescriptor()->FindMethodByName(message_info.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found " << replied->DebugString() ;
		return;
	}
	//gate 和其他服务器不一样，直接返回消息给客户端

	auto it = gate_tls.sessions().find(replied->session_id());
	if (it == gate_tls.sessions().end())
	{
		LOG_ERROR << "conn id not found  session id "  << "," << replied->session_id();
		return;
	}

	MessageBody message;
	message.set_body(replied->body());
	message.set_id(replied->id());
	message.set_message_id(route_data.message_id());
	g_gate_node->codec().send(conn, message);
	g_gate_node->Send2Client(it->second.conn_, message);

	///<<< END WRITING YOUR CODE
}

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}
