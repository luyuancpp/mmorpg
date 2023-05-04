#include "scene_server_player_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/system/player_scene_system.h"
///<<< END WRITING YOUR CODE
void ServerPlayerSceneServiceHandler::EnterSceneController2Gs(entt::entity player,
	const ::Controller2GsEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	//todo������gate Ȼ��ſ��Կ�ʼ���Ը��ͻ��˷�����Ϣ��, gs��Ϣ˳������Ҫע�⣬����a, �ٽ���b gs����ͻ�����Ϣ��˳��һ��
	PlayerSceneSystem::EnterScene(player, request->scene_id());
///<<< END WRITING YOUR CODE
}
