#include "player_util.h"

#include "muduo/base/Logging.h"

#include "game_logic/network//message_util.h"

#include "scene/util/scene_util.h"
#include "thread_local/storage_common_logic.h"

#include "service/scene_player_service.h"


void PlayerSceneUtil::HandleEnterScene(entt::entity player, entt::entity scene)
{
	const auto sceneInfo = tls.sceneRegistry.try_get<SceneInfo>(scene);
	if (sceneInfo == nullptr)
	{
		LOG_ERROR << "Failed to get scene info for player: " << tls.registry.get<Guid>(player);
		return;
	}

	EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(*sceneInfo);
	SendMessageToPlayer(ClientPlayerSceneServiceNotifyEnterSceneMsgId, message, player);
}

void PlayerSceneUtil::HandleLeaveScene(entt::entity player)
{

}
