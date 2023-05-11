#include "controller_scene_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitControllerSceneServiceUpdateCrossMainSceneInfoRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<UpdateCrossMainSceneInfoResponse>(std::bind(&OnControllerSceneServiceUpdateCrossMainSceneInfoRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnControllerSceneServiceUpdateCrossMainSceneInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<UpdateCrossMainSceneInfoResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

