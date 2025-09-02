
#include "game_player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "core/network/message_system.h"
#include "player/system/player_node_system.h"
#include "player/system/player_scene_system.h"
#include "proto/scene/player_scene.pb.h"
#include "modules/scene/comp/scene_comp.h"
#include "modules/scene/system/scene_system.h"
#include "rpc/service_info/player_scene_service_info.h"
#include "network/player_message_utils.h"

///<<< END WRITING YOUR CODE



void SceneScenePlayerHandler::EnterScene(entt::entity player,const ::GsEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling GsEnterSceneRequest for player: " << tlsRegistryManager.actorRegistry.get<Guid>(player) << ", scene_id: " << request->scene_id();

	// 进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
	PlayerSceneSystem::HandleEnterScene(player, entt::to_entity(request->scene_id()));
///<<< END WRITING YOUR CODE

}


void SceneScenePlayerHandler::LeaveScene(entt::entity player,const ::GsLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_DEBUG << "Handling GsLeaveSceneRequest for player: " << tlsRegistryManager.actorRegistry.get<Guid>(player);
	SceneUtil::LeaveScene({ .leaver = player });
	if (request->change_scene_info().change_gs_type() == ChangeSceneInfoPBComponent::eDifferentGs) // 存储完毕以后才能换场景，防止回档
	{
		tlsRegistryManager.actorRegistry.emplace_or_replace<ChangeSceneInfoPBComponent>(player, request->change_scene_info());
		// 离开gs 清除session
		PlayerNodeSystem::HandleExitGameNode(player);
		LOG_DEBUG << "Player " << tlsRegistryManager.actorRegistry.get<Guid>(player) << " session cleared after leaving scene.";
	}
///<<< END WRITING YOUR CODE

}


void SceneScenePlayerHandler::EnterSceneS2C(entt::entity player,const ::EnterSceneS2CRequest* request,
	::EnterScenerS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling EnterSceneS2CRequest for player: " << tlsRegistryManager.actorRegistry.get<Guid>(player);

	const auto sceneEntity = tlsRegistryManager.actorRegistry.try_get<SceneEntityComp>(player);
	if (sceneEntity == nullptr)
	{
		LOG_ERROR << "Player " << tlsRegistryManager.actorRegistry.get<Guid>(player) << " has not entered any scene.";
		return;
	}

	::EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(tlsRegistryManager.actorRegistry.get<SceneInfoPBComponent>(sceneEntity->sceneEntity));
	SendMessageToClientViaGate(SceneSceneClientPlayerNotifyEnterSceneMessageId, message, player);
///<<< END WRITING YOUR CODE

}

