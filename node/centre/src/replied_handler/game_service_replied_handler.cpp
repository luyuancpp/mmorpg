#include "game_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE

#include <boost/get_pointer.hpp>
#include <muduo/base/Logging.h>

#include "src/system/centre_player_system.h"
#include "src/thread_local/thread_local_storage.h"
#include "service/service.h"
#include "src/replied_handler/player_service_replied.h"
#include "src/thread_local/centre_thread_local_storage.h"
#include "src/thread_local/thread_local_storage_common_logic.h"

#include "component_proto/player_network_comp.pb.h"

///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitGameServiceEnterGsRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceEnterGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeRouteMessageResponse>(std::bind(&OnGameServiceSend2PlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<GameNodeRpcClientResponse>(std::bind(&OnGameServiceClientSend2PlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceGateConnectGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceCentreSend2PlayerViaGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeRouteMessageResponse>(std::bind(&OnGameServiceCallPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGameServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGameServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceUpdateSessionRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceEnterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<CreateSceneResponse>(std::bind(&OnGameServiceCreateSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGameServiceEnterGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE

	
///<<< END WRITING YOUR CODE
}

void OnGameServiceSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceClientSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GameNodeRpcClientResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceGateConnectGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceCentreSend2PlayerViaGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceCallPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	if (replied->msg().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << replied->msg().message_id() ;
		return;
	}
	auto sid = entt::to_entity(replied->ex().session_id());
	if (!tls.session_registry.valid(sid))
	{
		LOG_ERROR << "can not find session id " << replied->ex().session_id();
		return;
	}
	const auto player_info = 
		tls.session_registry.try_get<PlayerSessionInfo>(sid);
	if (nullptr == player_info)
	{
		LOG_ERROR << "session not found " << replied->ex().session_id();
		return;
	}
	auto player_id = player_info->player_id();
	const auto& message_info = g_message_info.at(replied->msg().message_id() );

    auto player = cl_tls.get_player(player_id);
	if (tls.registry.valid(player))
	{
		LOG_ERROR << "PlayerService player not found " << player_id << ", message id"
			<< replied->msg().message_id();
		return;
	}
	const auto service_it = g_player_service_replied.find(message_info.service);
	if (service_it == g_player_service_replied.end())
	{
		LOG_ERROR << "PlayerService service not found " << player_id << ","
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
	if (!player_response->ParsePartialFromArray(replied->msg().body().data(), int32_t(replied->msg().body().size())))
	{
        LOG_ERROR << "ParsePartialFromArray " << message_info.method;
        return;
	}
	service_impl->CallMethod(method, player, nullptr, boost::get_pointer(player_response));
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

void OnGameServiceUpdateSessionRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceEnterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceCreateSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CreateSceneResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

