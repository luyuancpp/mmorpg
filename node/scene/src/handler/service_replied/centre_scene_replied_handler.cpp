
#include "centre_scene_replied_handler.h"

#include "service_info//centre_scene_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE



void InitCentreSceneServiceRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::RegisterSceneResponse>(CentreSceneServiceRegisterSceneMessageId,
        std::bind(&OnCentreSceneServiceRegisterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreSceneServiceUnRegisterSceneMessageId,
        std::bind(&OnCentreSceneServiceUnRegisterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnCentreSceneServiceRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterSceneResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreSceneServiceUnRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
