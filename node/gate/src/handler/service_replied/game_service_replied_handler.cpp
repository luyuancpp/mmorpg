
#include "game_service_replied_handler.h"

#include "service_info//game_service_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
#include "gate_node.h"
#include "thread_local/storage_gate.h"

///<<< END WRITING YOUR CODE



void InitSceneRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::Empty>(ScenePlayerEnterGameNodeMessageId,
        std::bind(&OnScenePlayerEnterGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(SceneSendMessageToPlayerMessageId,
        std::bind(&OnSceneSendMessageToPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::ClientSendMessageToPlayerResponse>(SceneClientSendMessageToPlayerMessageId,
        std::bind(&OnSceneClientSendMessageToPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(SceneCentreSendToPlayerViaGameNodeMessageId,
        std::bind(&OnSceneCentreSendToPlayerViaGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(SceneInvokePlayerServiceMessageId,
        std::bind(&OnSceneInvokePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(SceneRouteNodeStringMsgMessageId,
        std::bind(&OnSceneRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(SceneRoutePlayerStringMsgMessageId,
        std::bind(&OnSceneRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(SceneUpdateSessionDetailMessageId,
        std::bind(&OnSceneUpdateSessionDetailRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(SceneEnterSceneMessageId,
        std::bind(&OnSceneEnterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::CreateSceneResponse>(SceneCreateSceneMessageId,
        std::bind(&OnSceneCreateSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RegisterNodeSessionResponse>(SceneRegisterNodeSessionMessageId,
        std::bind(&OnSceneRegisterNodeSessionRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnScenePlayerEnterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneClientSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::ClientSendMessageToPlayerResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneCentreSendToPlayerViaGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneInvokePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneUpdateSessionDetailRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneEnterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneCreateSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::CreateSceneResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnSceneRegisterNodeSessionRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterNodeSessionResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
