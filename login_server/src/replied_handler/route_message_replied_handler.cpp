#include "src/comp/account_player.h"
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/game_logic/tips_id.h"
#include "src/login_server.h"
#include "src/network/node_info.h"
#include "src/network/route_system.h"
#include "src/pb/pbc/database_service_service.h"
#include "src/pb/pbc/service.h"
#include "src/thread_local/login_thread_local_storage.h"
#include "src/util/defer.h"

#include "controller_service_service.h"
#include "gate_service_service.h"
#include "src/pb/pbc/common_proto/database_service.pb.h"

extern ProtobufDispatcher g_response_dispatcher;

void OnServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	//函数返回前一定会执行的函数
	defer(cl_tls.set_next_route_node_type(UINT32_MAX));
	defer(cl_tls.set_next_route_node_id(UINT32_MAX));
	defer(cl_tls.set_current_session_id(kInvalidSessionId));

    cl_tls.set_current_session_id(replied->session_id());

	if (replied->route_data_list().empty())
	{
		LOG_ERROR << "msg list empty:" << replied->DebugString();
		return;
	}
	//todo find all service
	if (replied->route_data_list().rbegin()->message_id() >= g_message_info.size() )
	{
		LOG_ERROR << "message_id not found " << replied->DebugString();
		return;
	}
	auto& message_info = g_message_info[replied->route_data_list().rbegin()->message_id()];
	if (nullptr == message_info.service_impl_instance_)
	{
		LOG_ERROR << "message_id not found " << replied->DebugString();
		return;
	}
	auto& service = message_info.service_impl_instance_;
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found" << replied->DebugString();
		return;
	}
	//当前节点的请求信息
	std::shared_ptr<google::protobuf::Message> current_node_response(service->GetResponsePrototype(method).New());
	if (!current_node_response->ParseFromString(replied->body()))
	{
		LOG_ERROR << "invalid  body response" << replied->DebugString();
		return;
	}

	auto mutable_replied = const_cast<::RouteMsgStringResponse*>(replied.get());


	//当前节点的真正回复的消息
	g_response_dispatcher.onProtobufMessage(conn, current_node_response, timestamp);

	//处理完以后要删除当前节点的信息
	mutable_replied->mutable_route_data_list()->RemoveLast();

	//没有发送到下个节点就是要回复了
	if (cl_tls.next_route_node_type() == UINT32_MAX)
	{
		//已经是最后一个节点了
		if (mutable_replied->route_data_list().empty())
		{
			return;
		}
		auto& prev_route_data = *replied->route_data_list().rbegin();
		mutable_replied->set_body(current_node_response->SerializeAsString());
		mutable_replied->set_session_id(mutable_replied->session_id());
		mutable_replied->set_id(mutable_replied->id());
		switch (prev_route_data.node_info().node_type())
		{
		case kGateNode:
		{
			//todo test 节点不存在了消息会不会存留
			auto gate_it = login_tls.gate_nodes().find(prev_route_data.node_info().node_id());
			if (gate_it == login_tls.gate_nodes().end())
			{
				LOG_ERROR << "gate not found node id " << prev_route_data.node_info().node_id() << " " << replied->DebugString();
				return;
			}
			gate_it->second->session_.SendRouteResponse(GateServiceRouteNodeStringMsgMsgId,
				replied->id(),
				mutable_replied->SerializeAsString());
		}
		break;

		default:
		{
			LOG_INFO << "route to next node type error " << replied->DebugString() << "," << prev_route_data.node_info().node_type();
		}
		break;
		}
		return;
	}
	else
	{
		RouteMsgStringRequest request;
		request.set_body(cl_tls.route_msg_body());
		for (auto& request_data_it : mutable_replied->route_data_list())
		{
			*request.add_route_data_list() = request_data_it;
		}
		request.set_session_id(mutable_replied->session_id());
		request.set_id(mutable_replied->id());

		auto send_route_data = request.add_route_data_list();
		send_route_data->CopyFrom(cl_tls.route_data());
		send_route_data->mutable_node_info()->CopyFrom(g_login_node->node_info());
		switch (cl_tls.next_route_node_type())
		{
		case kControllerNode:
		{
			//发送到下个节点
			g_login_node->controller_node()->Route2Node(ControllerServiceRouteNodeStringMsgMsgId, request);
		}
		break;
		case kDatabaseNode:
		{
			//发送到下个节点
			g_login_node->db_node()->Route2Node(DbServiceRouteNodeStringMsgMsgId, request);
		}
		break;
		default:
		{
			LOG_ERROR << "route to next node type error " << request.DebugString() << "," << cl_tls.next_route_node_type();
		}
		break;
		}
	}


	///<<< END WRITING YOUR CODE
}

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}
