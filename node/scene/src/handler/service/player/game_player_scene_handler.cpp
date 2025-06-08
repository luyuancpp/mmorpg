
#include "game_player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "core/network/message_system.h"
#include "player/system/player_node_system.h"
#include "player/system/player_scene_system.h"
#include "proto/scene/player_scene.pb.h"
#include "scene/comp/scene_comp.h"
#include "scene/system/scene_system.h"
#include "service_info/player_scene_service_info.h"
///<<< END WRITING YOUR CODE



void GamePlayerSceneServiceHandler::EnterScene(entt::entity player,const ::GsEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling GsEnterSceneRequest for player: " << tls.registry.get<Guid>(player) << ", scene_id: " << request->scene_id();

	// 进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
	PlayerSceneSystem::HandleEnterScene(player, entt::to_entity(request->scene_id()));
///<<< END WRITING YOUR CODE


}



void GamePlayerSceneServiceHandler::LeaveScene(entt::entity player,const ::GsLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling GsLeaveSceneRequest for player: " << tls.registry.get<Guid>(player);

	PlayerSceneSystem::HandleLeaveScene(player);
	SceneUtil::LeaveScene({ .leaver = player });
	if (request->change_gs()) // 存储完毕以后才能换场景，防止回档
	{
		// 离开gs 清除session
		PlayerNodeSystem::HandleExitGameNode(player);
		LOG_INFO << "Player " << tls.registry.get<Guid>(player) << " session cleared after leaving scene.";
	}
///<<< END WRITING YOUR CODE


}



void GamePlayerSceneServiceHandler::EnterSceneS2C(entt::entity player,const ::EnterSceneS2CRequest* request,
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
	message.mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfoPBComponent>(sceneEntity->sceneEntity));
	SendMessageToPlayer(ClientPlayerSceneServiceNotifyEnterSceneMessageId, message, player);
///<<< END WRITING YOUR CODE


}


