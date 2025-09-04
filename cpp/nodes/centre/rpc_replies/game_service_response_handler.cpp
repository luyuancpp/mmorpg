
#include "game_service_response_handler.h"

#include "rpc/service_metadata/game_service_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE

#include <boost/get_pointer.hpp>
#include <muduo/base/Logging.h>


#include "rpc/service_metadata/service_metadata.h"
#include "rpc/player_rpc_response_handler.h"
#include "threading/redis_manager.h"
#include "type_alias/player_session_type_alias.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "threading/player_manager.h"
///<<< END WRITING YOUR CODE



void InitSceneReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(ScenePlayerEnterGameNodeMessageId,
        std::bind(&OnScenePlayerEnterGameNodeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(SceneSendMessageToPlayerMessageId,
        std::bind(&OnSceneSendMessageToPlayerReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::ClientSendMessageToPlayerResponse>(SceneClientSendMessageToPlayerMessageId,
        std::bind(&OnSceneClientSendMessageToPlayerReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(SceneCentreSendToPlayerViaGameNodeMessageId,
        std::bind(&OnSceneCentreSendToPlayerViaGameNodeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(SceneInvokePlayerServiceMessageId,
        std::bind(&OnSceneInvokePlayerServiceReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(SceneRouteNodeStringMsgMessageId,
        std::bind(&OnSceneRouteNodeStringMsgReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(SceneRoutePlayerStringMsgMessageId,
        std::bind(&OnSceneRoutePlayerStringMsgReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(SceneUpdateSessionDetailMessageId,
        std::bind(&OnSceneUpdateSessionDetailReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(SceneEnterSceneMessageId,
        std::bind(&OnSceneEnterSceneReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::CreateSceneResponse>(SceneCreateSceneMessageId,
        std::bind(&OnSceneCreateSceneReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeHandshakeResponse>(SceneNodeHandshakeMessageId,
        std::bind(&OnSceneHandshakeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnScenePlayerEnterGameNodeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneSendMessageToPlayerReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneClientSendMessageToPlayerReply(const TcpConnectionPtr& conn, const std::shared_ptr<::ClientSendMessageToPlayerResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneCentreSendToPlayerViaGameNodeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneInvokePlayerServiceReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	if (replied->message_content().message_id() >= gRpcServiceRegistry.size())
	{
		LOG_ERROR << "message_id not found " << replied->message_content().message_id() ;
		return;
	}

	const auto it = GlobalSessionList().find(replied->header().session_id());
	if (it == GlobalSessionList().end())
	{
		LOG_ERROR << "can not find session id " << replied->header().session_id();
		return;
	}

	const auto  player_id    = it->second;
	const auto& message_info = gRpcServiceRegistry.at(replied->message_content().message_id() );
	const auto  player = GetPlayer(player_id);
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "PlayerService player not found " << player_id << ", message id"
			<< replied->message_content().message_id();
		return;
	}

	const auto serviceIt = gPlayerServiceReplied.find(message_info.serviceName);
	if (serviceIt == gPlayerServiceReplied.end())
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

void OnSceneRouteNodeStringMsgReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneRoutePlayerStringMsgReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneUpdateSessionDetailReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneEnterSceneReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneCreateSceneReply(const TcpConnectionPtr& conn, const std::shared_ptr<::CreateSceneResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneHandshakeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
