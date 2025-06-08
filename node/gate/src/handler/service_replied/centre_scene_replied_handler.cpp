
#include "centre_scene_replied_handler.h"

#include "service_info//centre_scene_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE



void InitCentreSceneRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::RegisterSceneResponse>(CentreSceneRegisterSceneMessageId,
        std::bind(&OnCentreSceneRegisterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreSceneUnRegisterSceneMessageId,
        std::bind(&OnCentreSceneUnRegisterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnCentreSceneRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterSceneResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreSceneUnRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
