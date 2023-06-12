#include "game_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE

#include <boost/get_pointer.hpp>
#include <muduo/base/Logging.h>

#include "src/comp/player_list.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/pb/pbc/service.h"
#include "src/replied_handler/player_service_replied.h"
#include "src/thread_local/controller_thread_local_storage.h"

///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitGameServiceEnterGsRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGameServiceEnterGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeServiceMessageResponse>(std::bind(&OnGameServiceSend2PlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<GameNodeRpcClientResponse>(std::bind(&OnGameServiceClientSend2PlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGameServiceDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGameServiceGateConnectGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGameServiceControllerSend2PlayerViaGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeServiceMessageResponse>(std::bind(&OnGameServiceCallPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGameServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGameServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGameServiceUpdateSessionRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGameServiceEnterGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE

	
///<<< END WRITING YOUR CODE
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

void OnGameServiceDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceGateConnectGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceControllerSend2PlayerViaGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceCallPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	if (replied->msg().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << replied->msg().message_id() ;
		return;
	}
	auto session_it = controller_tls.gate_sessions().find(replied->ex().session_id());
	if (session_it == controller_tls.gate_sessions().end())
	{
		LOG_ERROR << "session not found " << replied->ex().session_id();
		return;
	}
	const auto try_session_player_id = tls.registry.try_get<Guid>(session_it->second);
	if (nullptr == try_session_player_id)
	{
		LOG_ERROR << "session not found " << replied->ex().session_id();
		return;
	}
	const auto& message_info = g_message_info.at(replied->msg().message_id() );
	const auto player_it = controller_tls.player_list().find(*try_session_player_id);
	if (player_it == controller_tls.player_list().end())
	{
		LOG_ERROR << "PlayerService player not found " << *try_session_player_id << ", message id"
			<< replied->msg().message_id();
		return;
	}
	const auto service_it = g_player_service_replied.find(message_info.service);
	if (service_it == g_player_service_replied.end())
	{
		LOG_ERROR << "PlayerService service not found " << *try_session_player_id << ","
		<< replied->msg().message_id();
		return;
	}
	const auto& service_impl = service_it->second;
	google::protobuf::Service* service = service_impl->service();
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.method);
	if (nullptr == method)
	{
		LOG_ERROR << "PlayerService method not found " << message_info.method;
		//todo client error;
		return;
	}
	const MessageUniquePtr player_response(service->GetResponsePrototype(method).New());
	player_response->ParseFromString(replied->msg().body());
	service_impl->CallMethod(method, player_it->second, nullptr, boost::get_pointer(player_response));
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

void OnGameServiceUpdateSessionRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

