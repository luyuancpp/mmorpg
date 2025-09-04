
#include "centre_scene_replied_handler.h"

#include "rpc/service_metadata/centre_scene_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE



void InitCentreSceneReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::RegisterSceneResponse>(CentreSceneRegisterSceneMessageId,
        std::bind(&OnCentreSceneRegisterSceneReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(CentreSceneUnRegisterSceneMessageId,
        std::bind(&OnCentreSceneUnRegisterSceneReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnCentreSceneRegisterSceneReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterSceneResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreSceneUnRegisterSceneReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
