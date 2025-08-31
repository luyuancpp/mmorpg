
#include "game_scene_replied_handler.h"

#include "rpc/service_info/game_scene_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE



void InitSceneSceneRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::Empty>(SceneSceneTestMessageId,
        std::bind(&OnSceneSceneTestRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnSceneSceneTestRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
