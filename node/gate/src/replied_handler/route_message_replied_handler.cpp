#include "muduo/base/Logging.h"

#include "src/thread_local/thread_local_storage_common_logic.h"
#include "src/gate_server.h"
#include "src/network/codec/dispatcher.h"
#include "src/thread_local/gate_thread_local_storage.h"
#include "src/util/defer.h"
#include "service/service.h"

#include "service/gate_service_service.h"

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
	//gate 和其他服务器不一样，直接返回消息给客户端
	entt::entity session_id{replied->session_id()};
	if (tls.session_registry.valid(session_id))
	{
		LOG_ERROR << "conn id not found  session id "  << "," << replied->session_id();
		return;
	}

	auto session = tls.session_registry.try_get<Session>(session_id);
	if (nullptr == session)
	{
        LOG_ERROR << "conn id not found  session id " << "," << replied->session_id();
        return;
	}

	MessageBody message;
	message.set_body(replied->body());
	message.set_id(replied->id());
	message.set_message_id(route_data.message_id());;
	g_gate_node->Send2Client(session->conn_, message);

	///<<< END WRITING YOUR CODE
}

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}
