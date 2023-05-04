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
	if (request->change_gs())//�洢����Ժ���ܻ���������ֹ�ص�
	{
		//�뿪gs ���session
		PlayerCommonSystem::RemovePlayereSession(player);
		PlayerCommonSystem::SavePlayer(player);
	}
///<<< END WRITING YOUR CODE
}
