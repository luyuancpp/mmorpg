#include "game_scene_server_player_handler.h"
#include "thread_local/storage.h"
#include "network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "comp/scene.h"
#include "component_proto/player_comp.pb.h"
#include "service/scene_client_player_service.h"
#include "system/player_node_system.h"
#include "system/player_scene_system.h"

#include "client_player_proto/scene_client_player.pb.h"
///<<< END WRITING YOUR CODE
void GamePlayerSceneServiceHandler::EnterScene(entt::entity player,
	const ::GsEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
//todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
	PlayerSceneSystem::EnterScene(player, request->scene_id());
///<<< END WRITING YOUR CODE
}

void GamePlayerSceneServiceHandler::LeaveScene(entt::entity player,
	const ::GsLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerSceneSystem::LeaveScene(player);
	if (request->change_gs())//存储完毕以后才能换场景，防止回档
	{
		//离开gs 清除session
		PlayerNodeSystem::SavePlayer(player);
		tls.registry.emplace<UnregisterPlayer>(player);
	}
///<<< END WRITING YOUR CODE
}

void GamePlayerSceneServiceHandler::EnterSceneS2C(entt::entity player,
	const ::EnterSceneS2CRequest* request,
	::EnterScenerS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	const auto scene_entity = tls.registry.try_get<SceneEntity>(player);
	if (nullptr == scene_entity)
	{
		LOG_ERROR << " player not enter scene ";
		return;
	}
	::EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfo>(scene_entity->scene_entity_));
	Send2Player(ClientPlayerSceneServicePushEnterSceneS2CMsgId, message, player);
///<<< END WRITING YOUR CODE
}

