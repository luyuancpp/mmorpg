#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "core/network/message_system.h"

#include "thread_local/redis_manager.h"

#include "service_info/player_scene_service_info.h"
#include "thread_local/storage.h"
#include "network/player_message_utils.h"
#include <thread_local/registry_manager.h>


void PlayerSceneSystem::HandleEnterScene(entt::entity player, entt::entity scene)
{
	const auto sceneInfo = tlsRegistryManager.sceneRegistry.try_get<SceneInfoPBComponent>(scene);
	if (sceneInfo == nullptr)
	{
		LOG_ERROR << "Failed to get scene info for player: " << tlsRegistryManager.actorRegistry.get<Guid>(player);
		return;
	}

	EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(*sceneInfo);
	SendMessageToClientViaGate(SceneSceneClientPlayerNotifyEnterSceneMessageId, message, player);
}

void PlayerSceneSystem::HandleBeforeLeaveScene(entt::entity player)
{

}
