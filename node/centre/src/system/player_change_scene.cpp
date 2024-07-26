#include "player_change_scene.h"

#include "constants/tips_id.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage.h"

#include "proto/logic/event/scene_event.pb.h"

//todo 各种服务器崩溃
void PlayerChangeSceneSystem::InitChangeSceneQueue(entt::entity player)
{
	tls.registry.emplace<PlayerCentreChangeSceneQueue>(player);
}

uint32_t PlayerChangeSceneSystem::PushChangeSceneInfo(entt::entity player, const CentreChangeSceneInfo& changeInfo)
{
	auto* const changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (nullptr == changeSceneQueue)
	{
		return kRetChangeScenePlayerQueueComponentNull;
	}
	CHECK_CONDITION(changeSceneQueue->changeSceneQueue.full(), kRetEnterSceneChangingGs)
		changeSceneQueue->changeSceneQueue.push_back(changeInfo);
	//todo
	changeSceneQueue->changeSceneQueue.front().set_change_time(muduo::Timestamp::now().secondsSinceEpoch());
	return kOK;
}

void PlayerChangeSceneSystem::PopFrontChangeSceneQueue(entt::entity player)
{
	auto* const changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (nullptr == changeSceneQueue)
	{
		return;
	}
	if (changeSceneQueue->changeSceneQueue.empty())
	{
		return;
	}
	changeSceneQueue->changeSceneQueue.pop_front();
}

void PlayerChangeSceneSystem::SetChangeGsStatus(entt::entity player, CentreChangeSceneInfo::eChangeGsStatus s)
{
	auto* const changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (nullptr == changeSceneQueue)
	{
		return;
	}
	if (changeSceneQueue->changeSceneQueue.empty())
	{
		return;
	}
	changeSceneQueue->changeSceneQueue.front().set_change_gs_status(s);
}

void PlayerChangeSceneSystem::CopySceneInfoToChangeInfo(CentreChangeSceneInfo& changeInfo, const SceneInfo& sceneInfo)
{
	changeInfo.set_scene_confid(sceneInfo.scene_confid());
	changeInfo.set_dungen_confid(sceneInfo.dungen_confid());
	changeInfo.set_guid(sceneInfo.guid());
	changeInfo.set_mirror_confid(sceneInfo.mirror_confid());
}

void PlayerChangeSceneSystem::TryProcessChangeSceneQueue(entt::entity player)
{
	auto* const tryChangeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (nullptr == tryChangeSceneQueue)
	{
		return;
	}
	auto& changeSceneQueue = tryChangeSceneQueue->changeSceneQueue;
	if (changeSceneQueue.empty())
	{
		return;
	}

	if (changeSceneQueue.front().change_gs_type() == CentreChangeSceneInfo::eSameGs)
	{
		//就算同gs,队列有消息也不能直接切换，
		ProcessSameGsChangeScene(player, changeSceneQueue.front());
	}
	else if (changeSceneQueue.front().change_gs_type() == CentreChangeSceneInfo::eDifferentGs)
	{
		ProcessDifferentGsChangeScene(player, changeSceneQueue.front());
	}
}

void PlayerChangeSceneSystem::ProcessSameGsChangeScene(entt::entity player, const CentreChangeSceneInfo& changeInfo)
{
	const auto& destScene = entt::entity{ changeInfo.guid() };
	//场景不存在了把消息删除,这个文件一定要注意这个队列各种异常情况
	if (entt::null == destScene)
	{
		//场景崩溃了，不用换了，玩家不切换，直接从队列里面删除
		PopFrontChangeSceneQueue(player);
		return;
	}
	ScenesSystem::LeaveScene({ player });
	ScenesSystem::EnterScene({ destScene , player });
	PopFrontChangeSceneQueue(player);
	OnEnterSceneOk(player);
}

void PlayerChangeSceneSystem::ProcessDifferentGsChangeScene(entt::entity player, const CentreChangeSceneInfo& changeInfo)
{
	//正在切换
	//切换gs  存储完毕之后才能进入下一个场景
	//放到存储完毕切换场景的队列里面，如果等够足够时间没有存储完毕，可能就是服务器崩溃了,注意，是可能
	if (changeInfo.change_gs_status() == CentreChangeSceneInfo::eLeaveGsScene)
	{
		ScenesSystem::LeaveScene({ player });
	}
	else if (changeInfo.change_gs_status() == CentreChangeSceneInfo::eEnterGsSceneSucceed)
	{
		//场景不存在了把消息删除,这个文件一定要注意这个队列各种异常情况
		const auto destScene = entt::entity{ changeInfo.guid() };
		if (entt::null == destScene)
		{
			//todo 考虑直接删除了会不会有异常
			//这时候gate已经更新了新的game node id 又进不去新场景,那我应该让他回到老场景
			ScenesSystem::EnterDefaultScene({ player });
		}
		else
		{
			ScenesSystem::EnterScene({ destScene, player });
		}
	}
	else if (changeInfo.change_gs_status() == CentreChangeSceneInfo::eGateEnterGsSceneSucceed)
	{
		PopFrontChangeSceneQueue(player);
		OnEnterSceneOk(player);
	}
}

void PlayerChangeSceneSystem::OnEnterSceneOk(entt::entity player)
{
	S2CEnterScene ev;
	ev.set_entity(entt::to_integral(player));
	tls.dispatcher.trigger(ev);
}
