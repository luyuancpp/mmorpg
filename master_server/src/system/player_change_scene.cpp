#include "player_change_scene.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/tips_id.h"
#include "src/network/player_session.h"
#include "src/system/player_scene_system.h"


void PlayerChangeSceneSystem::InitChangeSceneQueue(entt::entity player)
{
	registry.emplace<PlayerMsChangeSceneQueue>(player);
}

uint32_t PlayerChangeSceneSystem::ChangeScene(entt::entity player, const MsChangeSceneInfo&& change_info)
{
	GetPlayerCompnentReturnError(try_change_scene_queue, PlayerMsChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
	auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
	CheckCondtion(change_scene_queue.full(), kRetChangeScenePlayerQueueCompnentFull);
	change_scene_queue.push_back(change_info);
	return kRetOK;
}

uint32_t PlayerChangeSceneSystem::ChangeSameGsScene(entt::entity player)
{
	auto p_player_gs = registry.try_get<PlayerSession>(player);
	if ( nullptr == p_player_gs)
	{
		LOG_ERROR << "gs session null" << registry.get<Guid>(player);
		return kRetChangeScenePlayerQueueCompnentGsNull;
	}

	GetPlayerCompnentReturnError(try_change_scene_queue, PlayerMsChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
	auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
	if (change_scene_queue.empty())
	{
		return kRetChangeScenePlayerQueueCompnentEmpty;
	}
	auto& front_change = change_scene_queue.front();

	auto to_scene = ScenesSystem::GetSingleton().get_scene(front_change.scene_info().scene_id());
	if (entt::null == to_scene)//场景不存在了把消息删除,这个文件一定要注意这个队列各种异常情况
	{
		change_scene_queue.pop_front();
		return kRetEnterSceneSceneNotFound;
	}
	LeaveSceneParam lp;
	lp.leaver_ = player;
	ScenesSystem::GetSingleton().LeaveScene(lp);

	PlayerSceneSystem::OnLeaveScene(player, false);
	EnterSceneParam ep;
	ep.enterer_ = player;
	ep.scene_ = to_scene;
	ScenesSystem::GetSingleton().EnterScene(ep);
	PlayerSceneSystem::OnEnterScene(player);

	change_scene_queue.pop_front();//切换成功消息删除

	return kRetOK;
}

uint32_t PlayerChangeSceneSystem::ChangeDiffrentGsScene(entt::entity player)
{
	return kRetOK;
}

void PlayerChangeSceneSystem::OnEnterGsSuccess()
{

}
