#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "game_logic/core/network/message_system.h"

#include "scene/util/scene_util.h"
#include "thread_local/storage_common_logic.h"

#include "service_info/player_scene_service_info.h"


void PlayerSceneSystem::HandleEnterScene(entt::entity player, entt::entity scene)
{
	const auto sceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComponent>(scene);
	if (sceneInfo == nullptr)
	{
		LOG_ERROR << "Failed to get scene info for player: " << tls.registry.get<Guid>(player);
		return;
	}

	EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(*sceneInfo);
	SendMessageToPlayer(ClientPlayerSceneServiceNotifyEnterSceneMessageId, message, player);
}

void PlayerSceneSystem::HandleLeaveScene(entt::entity player)
{

}

