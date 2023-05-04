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
	//todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
	PlayerSceneSystem::EnterScene(player, request->scene_id());
///<<< END WRITING YOUR CODE
}
