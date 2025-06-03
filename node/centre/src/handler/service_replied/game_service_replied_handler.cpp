
#include "game_service_replied_handler.h"

#include "service_info//game_service_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE

#include <boost/get_pointer.hpp>
#include <muduo/base/Logging.h>

#include "thread_local/storage.h"
#include "service_info/service_info.h"
#include "service/player_service_replied.h"
#include "thread_local/storage_centre.h"
#include "thread_local/storage_common_logic.h"
#include "type_alias/player_session_type_alias.h"

#include "proto/logic/component/player_network_comp.pb.h"

///<<< END WRITING YOUR CODE



void InitGameServicePlayerEnterGameNodeRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::Empty>(GameServicePlayerEnterGameNodeMessageId,
        std::bind(&OnGameServicePlayerEnterGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(GameServiceSendMessageToPlayerMessageId,
        std::bind(&OnGameServiceSendMessageToPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::ClientSendMessageToPlayerResponse>(GameServiceClientSendMessageToPlayerMessageId,
        std::bind(&OnGameServiceClientSendMessageToPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(GameServiceCentreSendToPlayerViaGameNodeMessageId,
        std::bind(&OnGameServiceCentreSendToPlayerViaGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(GameServiceInvokePlayerServiceMessageId,
        std::bind(&OnGameServiceInvokePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(GameServiceRouteNodeStringMsgMessageId,
        std::bind(&OnGameServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(GameServiceRoutePlayerStringMsgMessageId,
        std::bind(&OnGameServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(GameServiceUpdateSessionDetailMessageId,
        std::bind(&OnGameServiceUpdateSessionDetailRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(GameServiceEnterSceneMessageId,
        std::bind(&OnGameServiceEnterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::CreateSceneResponse>(GameServiceCreateSceneMessageId,
        std::bind(&OnGameServiceCreateSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RegisterNodeSessionResponse>(GameServiceRegisterNodeSessionMessageId,
        std::bind(&OnGameServiceRegisterNodeSessionRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGameServicePlayerEnterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGameServiceSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGameServiceClientSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::ClientSendMessageToPlayerResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGameServiceCentreSendToPlayerViaGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGameServiceInvokePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	if (replied->message_content().message_id() >= gMessageInfo.size())
	{
		LOG_ERROR << "message_id not found " << replied->message_content().message_id() ;
		return;
	}

	const auto it = tlsSessions.find(replied->header().session_id());
	if (it == tlsSessions.end())
	{
		LOG_ERROR << "can not find session id " << replied->header().session_id();
		return;
	}

	const auto  player_id    = it->second.player_id();
	const auto& message_info = gMessageInfo.at(replied->message_content().message_id() );
	const auto  player = tlsCommonLogic.GetPlayer(player_id);
	if (!tls.registry.valid(player))
	{
		LOG_ERROR << "PlayerService player not found " << player_id << ", message id"
			<< replied->message_content().message_id();
		return;
	}

	const auto serviceIt = g_player_service_replied.find(message_info.serviceName);
	if (serviceIt == g_player_service_replied.end())
	{
		LOG_ERROR << "PlayerService service not found " << player_id << ","
		<< replied->message_content().message_id();
		return;
	}

	const auto& serviceImpl = serviceIt->second;
	google::protobuf::Service* service = serviceImpl->service();
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.methodName);
	if (nullptr == method)
	{
		LOG_ERROR << "PlayerService method not found " << message_info.methodName;
		return;
	}

	const MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());
	if (!playerResponse->ParsePartialFromArray(replied->message_content().serialized_message().data(),
		static_cast < int32_t > ( replied ->message_content( ).serialized_message( ). size ( ) )))
	{
        LOG_ERROR << "ParsePartialFromArray " << message_info.methodName;
        return;
	}

	serviceImpl->CallMethod(method, player, nullptr, boost::get_pointer(playerResponse));
///<<< END WRITING YOUR CODE

}

void OnGameServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGameServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGameServiceUpdateSessionDetailRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGameServiceEnterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGameServiceCreateSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::CreateSceneResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGameServiceRegisterNodeSessionRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterNodeSessionResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
