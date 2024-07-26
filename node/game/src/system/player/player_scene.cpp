#include "player_scene.h"

#include "muduo/base/Logging.h"

#include "system/network/message_system.h"
#include "system/scene/node_scene.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage_common_logic.h"

#include "service/scene_client_player_service.h"

void PlayerSceneSystem::EnterScene(entt::entity player, Guid scene)
{
	GameNodeSceneSystem::EnterScene({ entt::entity{scene}, player});
}

void PlayerSceneSystem::OnEnterScene(entt::entity player, entt::entity scene)
{
	const auto info = tls.sceneRegistry.try_get<SceneInfo>(scene);
	if (nullptr == info)
	{
		LOG_ERROR << "enter scene error" << tls.registry.get<Guid>(player);
		return;
	}
	EnterSceneS2C rq;
	rq.mutable_scene_info()->CopyFrom(*info);
	Send2Player(ClientPlayerSceneServicePushEnterSceneS2CMsgId, rq, player);
}

void PlayerSceneSystem::LeaveScene(entt::entity player)
{
	GameNodeSceneSystem::LeaveScene({player});
}

