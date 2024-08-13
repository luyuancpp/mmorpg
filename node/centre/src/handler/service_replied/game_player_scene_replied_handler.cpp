#include "game_player_scene_replied_handler.h"
#include "network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitGamePlayerSceneServiceEnterSceneRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGamePlayerSceneServiceEnterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<google::protobuf::Empty>(std::bind(&OnGamePlayerSceneServiceLeaveSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<EnterScenerS2CResponse>(std::bind(&OnGamePlayerSceneServiceEnterSceneS2CRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGamePlayerSceneServiceEnterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGamePlayerSceneServiceLeaveSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGamePlayerSceneServiceEnterSceneS2CRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<EnterScenerS2CResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

