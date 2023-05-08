#include "lobby_scene_replied_handler.h"
#include "src/network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitLobbyServiceStartCrossGsHandler()
{
	g_response_dispatcher.registerMessageCallback<StartCrossGsResponse>(std::bind(&OnLobbyServiceStartCrossGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<EnterCrossMainSceneResponese>(std::bind(&OnLobbyServiceEnterCrossMainSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<EnterCrossRoomSceneSceneWeightRoundRobinResponse>(std::bind(&OnLobbyServiceEnterCrossMainSceneWeightRoundRobinRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnLobbyServiceStartCrossGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartCrossGsResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLobbyServiceEnterCrossMainSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<EnterCrossMainSceneResponese>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLobbyServiceEnterCrossMainSceneWeightRoundRobinRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<EnterCrossRoomSceneSceneWeightRoundRobinResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

