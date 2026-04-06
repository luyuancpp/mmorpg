
#include "scene_admin_response_handler.h"

#include "rpc/service_metadata/scene_admin_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitSceneSceneReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(SceneSceneTestMessageId,
        std::bind(&OnSceneSceneTestReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnSceneSceneTestReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp)
{
    ///<<< BEGIN WRITING YOUR CODE
    ///<<< END WRITING YOUR CODE
}
