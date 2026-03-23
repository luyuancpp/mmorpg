
#include "game_player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "core/network/message_system.h"
#include "player/system/player_lifecycle.h"
#include "player/system/player_scene.h"
#include "proto/scene/player_scene.pb.h"
#include "modules/scene/comp/scene_comp.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
#include "network/player_message_utils.h"

///<<< END WRITING YOUR CODE

void SceneScenePlayerHandler::EnterScene(entt::entity player,const ::GsEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE

	// Player should already be loaded by PlayerEnterGameNode (Node Handler).
	// We just handle the logical entry into the specific scene instance here.
	if (tlsRegistryManager.actorRegistry.valid(player))
	{
		PlayerSceneSystem::HandleEnterScene(player, entt::to_entity(request->scene_id()));
	}
	else
	{
		LOG_ERROR << "Player entity not found in EnterScene handler. This should not happen if PlayerEnterGameNode was called first.";
	}
///<<< END WRITING YOUR CODE

}

void SceneScenePlayerHandler::LeaveScene(entt::entity player,const ::GsLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_DEBUG << "Handling GsLeaveSceneRequest for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);

	// Save player data to Redis and handle exit logic (cleanup, notify Centre)
	// This replaces the legacy ChangeSceneInfo state machine checks with a direct save-and-exit flow.
	PlayerLifecycleSystem::HandleExitGameNode(player);

///<<< END WRITING YOUR CODE

}

void SceneScenePlayerHandler::EnterSceneS2C(entt::entity player,const ::EnterSceneS2CRequest* request,
	::EnterSceneS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling EnterSceneS2CRequest for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);

	const auto sceneEntity = tlsRegistryManager.actorRegistry.try_get<SceneEntityComp>(player);
	if (sceneEntity == nullptr)
	{
		LOG_ERROR << "Player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) << " has not entered any scene.";
		return;
	}

	::EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<SceneInfoComp>(sceneEntity->sceneEntity));
	SendMessageToClientViaGate(SceneSceneClientPlayerNotifyEnterSceneMessageId, message, player);
///<<< END WRITING YOUR CODE

}
