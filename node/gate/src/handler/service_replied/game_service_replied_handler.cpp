
#include "game_service_replied_handler.h"

#include "service_info//game_service_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
#include "gate_node.h"
#include "thread_local/storage_gate.h"

///<<< END WRITING YOUR CODE



void InitGameServiceRepliedHandler()
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
    auto it = tls_gate.sessions().find(replied->session_id());
	if (it == tls_gate.sessions().end())
	{
		LOG_ERROR << "conn id not found  session id " << "," << replied->session_id();
		return;
	}
	gGateNode->Codec().send(it->second.conn, replied->message_content());
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
