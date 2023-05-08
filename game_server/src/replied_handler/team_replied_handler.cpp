#include "team_replied_handler.h"
#include "src/network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitTeamServiceEnterSceneHandler()
{
	g_response_dispatcher.registerMessageCallback<TeamTestS2SRespone>(std::bind(&OnTeamServiceEnterSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnTeamServiceEnterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<TeamTestS2SRespone>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

