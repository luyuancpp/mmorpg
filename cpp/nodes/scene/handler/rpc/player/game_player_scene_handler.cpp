
#include "game_player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "core/network/message_system.h"
#include "player/system/player_lifecycle.h"
#include "player/system/player_scene.h"
#include "proto/service/cpp/rpc/scene/player_scene.pb.h"
#include "modules/scene/comp/scene_comp.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
#include "network/player_message_utils.h"
#include <modules/scene/system/room_common.h>

///<<< END WRITING YOUR CODE



void SceneScenePlayerHandler::EnterScene(entt::entity player,const ::GsEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling GsEnterSceneRequest for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) << ", scene_id: " << request->scene_id();

	// 进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
	PlayerSceneSystem::HandleEnterScene(player, entt::to_entity(request->scene_id()));
///<<< END WRITING YOUR CODE

}


void SceneScenePlayerHandler::LeaveScene(entt::entity player,const ::GsLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_DEBUG << "Handling GsLeaveSceneRequest for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
	RoomCommon::LeaveRoom({ .leaver = player });

	const auto& changeInfo = request->change_scene_info();
	if (request->change_scene_info().change_gs_type() == ChangeRoomInfoPBComponent::eDifferentGs) // 存储完毕以后才能换场景，防止回档
	{
		// 检查 state 是否为允许切场景的状态
		if (changeInfo.state() != ChangeRoomInfoPBComponent::ePendingLeave &&
			changeInfo.state() != ChangeRoomInfoPBComponent::eLeaving) {
			LOG_ERROR << "Invalid change_scene state for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
			return;
		}

		tlsRegistryManager.actorRegistry.emplace_or_replace<ChangeRoomInfoPBComponent>(player, changeInfo);
		// 离开gs 清除session
		PlayerLifecycleSystem::HandleExitGameNode(player);
		LOG_DEBUG << "Player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) << " session cleared after leaving scene.";
	}
///<<< END WRITING YOUR CODE

}


void SceneScenePlayerHandler::EnterSceneS2C(entt::entity player,const ::EnterSceneS2CRequest* request,
	::EnterScenerS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling EnterSceneS2CRequest for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);

	const auto sceneEntity = tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(player);
	if (sceneEntity == nullptr)
	{
		LOG_ERROR << "Player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) << " has not entered any scene.";
		return;
	}

	::EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<RoomInfoPBComponent>(sceneEntity->roomEntity));
	SendMessageToClientViaGate(SceneSceneClientPlayerNotifyEnterSceneMessageId, message, player);
///<<< END WRITING YOUR CODE

}

