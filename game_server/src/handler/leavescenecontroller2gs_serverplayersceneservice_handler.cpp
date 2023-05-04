#include "scene_server_player_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/system/player_common_system.h"
#include "src/system/player_scene_system.h"
///<<< END WRITING YOUR CODE
void ServerPlayerSceneServiceHandler::LeaveSceneController2Gs(entt::entity player,
	const ::Controller2GsLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerSceneSystem::LeaveScene(player);
	if (request->change_gs())//存储完毕以后才能换场景，防止回档
	{
		//离开gs 清除session
		PlayerCommonSystem::RemovePlayereSession(player);
		PlayerCommonSystem::SavePlayer(player);
	}
///<<< END WRITING YOUR CODE
}
