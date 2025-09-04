
#include "game_scene_replied_handler.h"

#include "rpc/service_metadata/game_scene_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE



void InitSceneSceneRepliedHandler()
{
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(SceneSceneTestMessageId,
        std::bind(&OnSceneSceneTestRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnSceneSceneTestRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
