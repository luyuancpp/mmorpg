#include "src/comp/account_player.h"
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/game_logic/tips_id.h"
#include "src/login_server.h"
#include "src/network/node_info.h"
#include "src/network/route_system.h"
#include "src/thread_local/login_thread_local_storage.h"
#include "src/util/defer.h"
#include "src/pb/pbc/service.h"

#include "src/pb/pbc/common_proto/database_service.pb.h"
#include "controller_service_service.h"
#include "gate_service_service.h"

extern ProtobufDispatcher g_response_dispatcher;

void OnServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	defer(cl_tls.set_current_session_id(kInvalidSessionId));
	cl_tls.set_current_session_id(replied->session_id());

	//函数返回前一定会执行的函数
	defer(cl_tls.set_next_route_node_type(UINT32_MAX));
	defer(cl_tls.set_next_route_node_id(UINT32_MAX));
	defer(cl_tls.set_current_session_id(kInvalidSessionId));
	if (replied->route_data_list_size() <= 0)
	{
		LOG_ERROR << "msg list empty:" << replied->DebugString();
		return;
	}
	//todo find all service
	auto& route_data = replied->route_data_list(replied->route_data_list_size() - 1);
	auto sit = g_service_method_info.find(route_data.message_id());
	if (sit == g_service_method_info.end())
	{
		LOG_INFO << "service_method_id not found " << route_data.message_id();
		return;
	}
	const google::protobuf::MethodDescriptor* method = g_login_node->login_handler().GetDescriptor()->FindMethodByName(sit->second.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found" << replied->DebugString() << "method name" << route_data.method();
		return;
	}
	//当前节点的请求信息
	std::shared_ptr<google::protobuf::Message> current_node_response(g_login_node->login_handler().GetResponsePrototype(method).New());
	if (!current_node_response->ParseFromString(replied->body()))
	{
		LOG_ERROR << "invalid  body response" << replied->DebugString() << "method name" << route_data.method();
		return;
	}
	cl_tls.set_current_session_id(replied->session_id());
	//当前节点的真正回复的消息
	g_response_dispatcher.onProtobufMessage(conn, current_node_response, timestamp);

	auto mutable_replied = const_cast<::RouteMsgStringResponse*>(replied.get());
	mutable_replied->mutable_route_data_list()->RemoveLast();
	//已经是最后一个节点了
	if (mutable_replied->route_data_list_size() <= 0)
	{
		return;
	}
	//需要回复到前个节点
	auto prev_route_data = replied->route_data_list(replied->route_data_list_size() - 1);
	mutable_replied->set_body(cl_tls.route_msg_body());
	switch (cl_tls.next_route_node_type())
	{
	case kControllerNode:
	{

	}
	break;
	case kDatabaseNode:
	{

	}
	break;
	case kGateNode:
	{
		//todo test 节点不存在了消息会不会存留
		auto gate_it = login_tls.gate_nodes().find(cl_tls.next_route_node_id());
		if (gate_it == login_tls.gate_nodes().end())
		{
			LOG_ERROR << "gate not found node id " << cl_tls.next_route_node_id() << replied->DebugString();
			return;
		}
		gate_it->second->session_.CallMethod(GateServiceRouteNodeStringMsgMethod, mutable_replied);
	}
	break;

	default:
	{
		LOG_ERROR << "route to next node type error " << replied->DebugString() << "," << cl_tls.next_route_node_type();
	}
	break;
	}
	///<<< END WRITING YOUR CODE
}

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}
