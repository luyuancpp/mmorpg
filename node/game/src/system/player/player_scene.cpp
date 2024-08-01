#include "player_scene.h"

#include "muduo/base/Logging.h"

#include "system/network/message_system.h"
#include "game_logic/scene/system/game_node_scene_system.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage_common_logic.h"

#include "service/scene_client_player_service.h"

void PlayerSceneSystem::EnterScene(entt::entity player, Guid sceneId)
{
	GameNodeSceneSystem::EnterScene({ .scene = entt::entity{sceneId}, .enter = player });
}

void PlayerSceneSystem::OnEnterScene(entt::entity player, entt::entity scene)
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

void PlayerSceneSystem::LeaveScene(entt::entity player)
{
	GameNodeSceneSystem::LeaveScene({ player });
}
