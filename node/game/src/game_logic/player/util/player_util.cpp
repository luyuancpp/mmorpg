#include "player_util.h"

#include "muduo/base/Logging.h"

#include "game_logic/network//message_system.h"

#include "system/scene/scene_system.h"
#include "thread_local/storage_common_logic.h"

#include "service/scene_client_player_service.h"

void PlayerSceneUtil::EnterScene(entt::entity player, Guid sceneId)
{
	
}

void PlayerSceneUtil::OnEnterScene(entt::entity player, entt::entity scene)
{
	const auto sceneInfo = tls.sceneRegistry.try_get<SceneInfo>(scene);
	if (sceneInfo == nullptr)
	{
		LOG_ERROR << "Failed to get scene info for player: " << tls.registry.get<Guid>(player);
		return;
	}

	EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(*sceneInfo);
	SendMessageToPlayer(ClientPlayerSceneServicePushEnterSceneS2CMsgId, message, player);
}

void PlayerSceneUtil::LeaveScene(entt::entity player)
{

}
