#include "player_change_scene.h"

#include "constants/tips_id.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage.h"

#include "proto/logic/event/scene_event.pb.h"

//todo 各种服务器崩溃// 初始化场景切换队列
void PlayerChangeSceneSystem::InitChangeSceneQueue(entt::entity player) {
	tls.registry.emplace<PlayerCentreChangeSceneQueue>(player);
}

// 添加切换场景信息到队列
uint32_t PlayerChangeSceneSystem::PushChangeSceneInfo(entt::entity player, const CentreChangeSceneInfo& changeInfo) {
	auto* const changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (!changeSceneQueue) {
		return kRetChangeScenePlayerQueueComponentNull;  // 玩家队列组件为空
	}
	if (changeSceneQueue->changeSceneQueue.full()) {
		return kRetEnterSceneChangingGs;  // 进入场景切换中的游戏服务器
	}

	changeSceneQueue->changeSceneQueue.push_back(changeInfo);
	changeSceneQueue->changeSceneQueue.back().set_change_time(muduo::Timestamp::now().secondsSinceEpoch());
	return kOK;
}

// 移除队列中首个切换场景信息
void PlayerChangeSceneSystem::PopFrontChangeSceneQueue(entt::entity player) {
	auto* const changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (!changeSceneQueue) {
		return;
	}
	if (changeSceneQueue->changeSceneQueue.empty()) {
		return;
	}

	changeSceneQueue->changeSceneQueue.pop_front();
}

// 设置当前切换场景信息的切换状态
void PlayerChangeSceneSystem::SetChangeGsStatus(entt::entity player, CentreChangeSceneInfo::eChangeGsStatus s) {
	auto* const changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (!changeSceneQueue) {
		return;
	}
	if (changeSceneQueue->changeSceneQueue.empty()) {
		return;
	}

	changeSceneQueue->changeSceneQueue.front().set_change_gs_status(s);
}

// 将场景信息复制到切换场景信息中
void PlayerChangeSceneSystem::CopySceneInfoToChangeInfo(CentreChangeSceneInfo& changeInfo, const SceneInfo& sceneInfo) {
	changeInfo.set_scene_confid(sceneInfo.scene_confid());
	changeInfo.set_dungen_confid(sceneInfo.dungen_confid());
	changeInfo.set_guid(sceneInfo.guid());
	changeInfo.set_mirror_confid(sceneInfo.mirror_confid());
}

// 处理玩家的场景切换队列
void PlayerChangeSceneSystem::ProcessChangeSceneQueue(entt::entity player) {
	auto* const tryChangeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (!tryChangeSceneQueue) {
		return;
	}
	auto& changeSceneQueue = tryChangeSceneQueue->changeSceneQueue;
	if (changeSceneQueue.empty()) {
		return;
	}

	const auto& changeInfo = changeSceneQueue.front();
	if (changeInfo.change_gs_type() == CentreChangeSceneInfo::eSameGs) {
		ProcessSameGsChangeScene(player, changeInfo);
	}
	else if (changeInfo.change_gs_type() == CentreChangeSceneInfo::eDifferentGs) {
		ProcessDifferentGsChangeScene(player, changeInfo);
	}
}

// 处理同一游戏服务器内的场景切换
void PlayerChangeSceneSystem::ProcessSameGsChangeScene(entt::entity player, const CentreChangeSceneInfo& changeInfo) {
	const auto& destScene = entt::entity{ changeInfo.guid() };
	if (entt::null == destScene) {
		// 场景不存在，移除消息
		PopFrontChangeSceneQueue(player);
		return;
	}

	ScenesSystem::LeaveScene({ player });
	ScenesSystem::EnterScene({ destScene , player });
	PopFrontChangeSceneQueue(player);
	OnEnterSceneOk(player);
}

// 处理不同游戏服务器间的场景切换
void PlayerChangeSceneSystem::ProcessDifferentGsChangeScene(entt::entity player, const CentreChangeSceneInfo& changeInfo) {
	if (changeInfo.change_gs_status() == CentreChangeSceneInfo::eLeaveGsScene) {
		ScenesSystem::LeaveScene({ player });
	}
	else if (changeInfo.change_gs_status() == CentreChangeSceneInfo::eEnterGsSceneSucceed) {
		const auto destScene = entt::entity{ changeInfo.guid() };
		if (entt::null == destScene) {
			ScenesSystem::EnterDefaultScene({ player });
		}
		else {
			ScenesSystem::EnterScene({ destScene, player });
		}
	}
	else if (changeInfo.change_gs_status() == CentreChangeSceneInfo::eGateEnterGsSceneSucceed) {
		PopFrontChangeSceneQueue(player);
		OnEnterSceneOk(player);
	}
}

// 确认玩家成功进入场景后的操作
void PlayerChangeSceneSystem::OnEnterSceneOk(entt::entity player) {
	S2CEnterScene ev;
	ev.set_entity(entt::to_integral(player));
	tls.dispatcher.trigger(ev);
}
