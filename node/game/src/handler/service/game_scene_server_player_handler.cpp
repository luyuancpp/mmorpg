#include "game_scene_server_player_handler.h"
#include "thread_local/storage.h"
#include "system/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "comp/scene_comp.h"
#include "proto/logic/component/player_comp.pb.h"
#include "service/scene_client_player_service.h"
#include "game_logic/player/system/player_node_util.h"
#include "game_logic/player/system/player_util.h"

#include "proto/logic/client_player/scene_client_player.pb.h"
///<<< END WRITING YOUR CODE
void GamePlayerSceneServiceHandler::EnterScene(entt::entity player,
	const ::GsEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling GsEnterSceneRequest for player: " << tls.registry.get<Guid>(player) << ", scene_id: " << request->scene_id();

	// 进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
	PlayerSceneSystem::EnterScene(player, request->scene_id());
///<<< END WRITING YOUR CODE
}

void GamePlayerSceneServiceHandler::LeaveScene(entt::entity player,
	const ::GsLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling GsLeaveSceneRequest for player: " << tls.registry.get<Guid>(player);

	PlayerSceneSystem::LeaveScene(player);
	if (request->change_gs()) // 存储完毕以后才能换场景，防止回档
	{
		// 离开gs 清除session
		PlayerNodeSystem::SavePlayer(player);
		tls.registry.emplace<UnregisterPlayer>(player);
		LOG_INFO << "Player " << tls.registry.get<Guid>(player) << " session cleared after leaving scene.";
	}
///<<< END WRITING YOUR CODE
}

void GamePlayerSceneServiceHandler::EnterSceneS2C(entt::entity player,
	const ::EnterSceneS2CRequest* request,
	::EnterScenerS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling EnterSceneS2CRequest for player: " << tls.registry.get<Guid>(player);

	const auto sceneEntity = tls.registry.try_get<SceneEntityComp>(player);
	if (sceneEntity == nullptr)
	{
		LOG_ERROR << "Player " << tls.registry.get<Guid>(player) << " has not entered any scene.";
		return;
	}

	::EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfo>(sceneEntity->sceneEntity));
	SendMessageToPlayer(ClientPlayerSceneServicePushEnterSceneS2CMsgId, message, player);
///<<< END WRITING YOUR CODE
}

