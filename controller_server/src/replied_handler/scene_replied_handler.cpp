#include "scene_replied_handler.h"
#include "src/network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitServerSceneServiceUpdateCrossMainSceneInfoHandler()
{
	g_response_dispatcher.registerMessageCallback<UpdateCrossMainSceneInfoResponse>(std::bind(&OnServerSceneServiceUpdateCrossMainSceneInfoRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnServerSceneServiceUpdateCrossMainSceneInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<UpdateCrossMainSceneInfoResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

