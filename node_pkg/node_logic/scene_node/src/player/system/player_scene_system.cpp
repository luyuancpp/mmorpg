#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "core/network/message_system.h"

#include "thread_local/storage_common_logic.h"

#include "service_info/player_scene_service_info.h"
#include "thread_local/storage.h"
#include "util/player_message_utils.h"


void PlayerSceneSystem::HandleEnterScene(entt::entity player, entt::entity scene)
{
	const auto sceneInfo = tls.sceneRegistry.try_get<SceneInfoPBComponent>(scene);
	if (sceneInfo == nullptr)
	{
		LOG_ERROR << "Failed to get scene info for player: " << tls.actorRegistry.get<Guid>(player);
		return;
	}

	EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(*sceneInfo);
	SendMessageToClientViaGate(SceneSceneClientPlayerNotifyEnterSceneMessageId, message, player);
}

void PlayerSceneSystem::HandleLeaveScene(entt::entity player)
{

}

