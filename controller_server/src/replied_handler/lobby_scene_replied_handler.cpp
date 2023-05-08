#include "lobby_scene_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/tips_id.h"
#include "src/system/player_tip_system.h"
#include "src/system/player_change_scene.h"
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitLobbyServiceStartCrossGsRepliedHandler()
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

