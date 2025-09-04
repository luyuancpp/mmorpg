
#include "game_service_response_handler.h"

#include "rpc/service_metadata/game_service_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
#include "gate_node.h"
#include "session/manager/session_manager.h"

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
        std::bind(&OnSceneNodeHandshakeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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
    auto it = tlsSessionManager.sessions().find(replied->session_id());
	if (it == tlsSessionManager.sessions().end())
	{
		LOG_ERROR << "conn id not found  session id " << "," << replied->session_id();
		return;
	}
	gGateNode->Codec().send(it->second.conn, replied->message_content());
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

void OnSceneNodeHandshakeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	gNode->GetNodeRegistrationManager().OnHandshakeReplied(*replied);
///<<< END WRITING YOUR CODE
}
