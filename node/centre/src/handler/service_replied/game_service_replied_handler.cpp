#include "game_service_replied_handler.h"
#include "network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE

#include <boost/get_pointer.hpp>
#include <muduo/base/Logging.h>

#include "thread_local/storage.h"
#include "service/service.h"
#include "handler/service_replied/player_service_replied.h"
#include "thread_local/storage_centre.h"
#include "thread_local/storage_common_logic.h"
#include "type_alias/player_session_type_alias.h"

#include "proto/logic/component/player_network_comp.pb.h"

///<<< END WRITING YOUR CODE

void InitGameServicePlayerEnterGameNodeRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServicePlayerEnterGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeRouteMessageResponse>(std::bind(&OnGameServiceSendMessageToPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<PlayerClientResponse>(std::bind(&OnGameServiceClientSendMessageToPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceRegisterGateNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceCentreSendToPlayerViaGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<NodeRouteMessageResponse>(std::bind(&OnGameServiceInvokePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGameServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGameServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceUpdateSessionDetailRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGameServiceEnterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<CreateSceneResponse>(std::bind(&OnGameServiceCreateSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGameServicePlayerEnterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE

	
///<<< END WRITING YOUR CODE
}

void OnGameServiceSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceClientSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<PlayerClientResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceRegisterGateNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceCentreSendToPlayerViaGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGameServiceInvokePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	if (replied->body().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << replied->body().message_id() ;
		return;
	}
	const auto it = tlsSessions.find(replied->head().session_id());
	if (it == tlsSessions.end())
	{
		LOG_ERROR << "can not find session id " << replied->head().session_id();
		return;
	}
	const auto  player_id    = it->second.player_id();
	const auto& message_info = g_message_info.at(replied->body().message_id() );
	const auto  player = tlsCommonLogic.GetPlayer(player_id);
	if (tls.registry.valid(player))
	{
		LOG_ERROR << "PlayerService player not found " << player_id << ", message id"
			<< replied->body().message_id();
		return;
	}
	const auto service_it = g_player_service_replied.find(message_info.service);
	if (service_it == g_player_service_replied.end())
	{
		LOG_ERROR << "PlayerService service not found " << player_id << ","
		<< replied->body().message_id();
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
	if (!player_response->ParsePartialFromArray(replied->body().body().data(), 
		static_cast < int32_t > ( replied -> body ( ) . body ( ) . size ( ) )))
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

void OnGameServiceUpdateSessionDetailRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
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

