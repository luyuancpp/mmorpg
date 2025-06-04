
#include "game_scene_replied_handler.h"

#include "service_info//game_scene_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE



void InitGameSceneServiceRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::Empty>(GameSceneServiceTestMessageId,
        std::bind(&OnGameSceneServiceTestRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGameSceneServiceTestRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
