#include "centre_scene_replied_handler.h"
#include "network/codec/dispatcher.h"

extern ProtobufDispatcher gResponseDispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitCentreSceneServiceRegisterSceneRepliedHandler()
{
	gResponseDispatcher.registerMessageCallback<::RegisterSceneResponse>(std::bind(&OnCentreSceneServiceRegisterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	gResponseDispatcher.registerMessageCallback<::Empty>(std::bind(&OnCentreSceneServiceUnRegisterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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

