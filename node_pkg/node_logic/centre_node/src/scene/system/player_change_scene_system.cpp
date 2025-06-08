#include "player_change_scene_system.h"

#include "pbc/common_error_tip.pb.h"
#include "pbc/scene_error_tip.pb.h"
#include "scene/system/scene_system.h"
#include "thread_local/storage.h"
#include "muduo/base/Logging.h"
#include "proto/logic/event/scene_event.pb.h"
#include "time/system/time_system.h"

//todo 各种服务器崩溃// 初始化场景切换队列
void PlayerChangeSceneUtil::InitChangeSceneQueue(entt::entity player) {
	tls.registry.emplace<ChangeSceneQueuePBComponent>(player);
}

// 添加切换场景信息到队列
uint32_t PlayerChangeSceneUtil::PushChangeSceneInfo(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo) {
	auto* const changeSceneQueue = tls.registry.try_get<ChangeSceneQueuePBComponent>(player);
	if (!changeSceneQueue) {
		return kChangeScenePlayerQueueNotFound;  // 玩家队列组件为空
	}
	if (changeSceneQueue->changeSceneQueue.full()) {
		return kEnterSceneChangingGs;  // 进入场景切换中的游戏服务器
	}

	changeSceneQueue->changeSceneQueue.push_back(changeInfo);
	changeSceneQueue->changeSceneQueue.back().set_change_time(TimeUtil::NowSecondsUTC());
	return kSuccess;
}

// 移除队列中首个切换场景信息
void PlayerChangeSceneUtil::PopFrontChangeSceneQueue(entt::entity player) {
	auto* const changeSceneQueue = tls.registry.try_get<ChangeSceneQueuePBComponent>(player);
	if (!changeSceneQueue) {
		return;
	}
	if (changeSceneQueue->changeSceneQueue.empty()) {
		return;
	}

	changeSceneQueue->changeSceneQueue.pop_front();
}

// 设置当前切换场景信息的切换状态
void PlayerChangeSceneUtil::SetChangeSceneNodeStatus(entt::entity player, ChangeSceneInfoPBComponent::eChangeGsStatus s) {
	auto* const changeSceneQueue = tls.registry.try_get<ChangeSceneQueuePBComponent>(player);
	if (!changeSceneQueue) {
		return;
	}
	if (changeSceneQueue->changeSceneQueue.empty()) {
		return;
	}

	changeSceneQueue->changeSceneQueue.front().set_change_gs_status(s);
}

// 将场景信息复制到切换场景信息中
void PlayerChangeSceneUtil::CopySceneInfoToChangeInfo(ChangeSceneInfoPBComponent& changeInfo, const SceneInfoPBComponent& sceneInfo) {
	changeInfo.set_scene_confid(sceneInfo.scene_confid());
	changeInfo.set_dungen_confid(sceneInfo.dungen_confid());
	changeInfo.set_guid(sceneInfo.guid());
	changeInfo.set_mirror_confid(sceneInfo.mirror_confid());
}

// 处理玩家的场景切换队列
void PlayerChangeSceneUtil::ProcessChangeSceneQueue(entt::entity player) {
	auto* const tryChangeSceneQueue = tls.registry.try_get<ChangeSceneQueuePBComponent>(player);
	if (!tryChangeSceneQueue) {
		return;
	}
	auto& changeSceneQueue = tryChangeSceneQueue->changeSceneQueue;
	if (changeSceneQueue.empty()) {
		return;
	}

	const auto& changeInfo = changeSceneQueue.front();
	if (changeInfo.change_gs_type() == ChangeSceneInfoPBComponent::eSameGs) {
		ProcessSameGsChangeScene(player, changeInfo);
	}
	else if (changeInfo.change_gs_type() == ChangeSceneInfoPBComponent::eDifferentGs) {
		ProcessDifferentGsChangeScene(player, changeInfo);
	}
}

// 处理同一游戏服务器内的场景切换
void PlayerChangeSceneUtil::ProcessSameGsChangeScene(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo) {
	const auto& destScene = entt::entity{ changeInfo.guid() };
	if (entt::null == destScene) {
		// 场景不存在，移除消息
		PopFrontChangeSceneQueue(player);
		return;
	}

	SceneUtil::LeaveScene({ player });
	SceneUtil::EnterScene({ destScene , player });
	PopFrontChangeSceneQueue(player);
	OnEnterSceneOk(player);
}

// 处理不同游戏服务器间的场景切换
void PlayerChangeSceneUtil::ProcessDifferentGsChangeScene(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo) {
	if (changeInfo.change_gs_status() == ChangeSceneInfoPBComponent::eLeaveGsScene) {
		SceneUtil::LeaveScene({ player });
	}
	else if (changeInfo.change_gs_status() == ChangeSceneInfoPBComponent::eEnterGsSceneSucceed) {
		const auto destScene = entt::entity{ changeInfo.guid() };
		if (entt::null == destScene) {
			SceneUtil::EnterDefaultScene({ player });
		}
		else {
			SceneUtil::EnterScene({ destScene, player });
		}
	}
	else if (changeInfo.change_gs_status() == ChangeSceneInfoPBComponent::eGateEnterGsSceneSucceed) {
		PopFrontChangeSceneQueue(player);
		OnEnterSceneOk(player);
	}
}

// 确认玩家成功进入场景后的操作
void PlayerChangeSceneUtil::OnEnterSceneOk(entt::entity player) {
	S2CEnterScene ev;
	ev.set_entity(entt::to_integral(player));
	tls.dispatcher.trigger(ev);
}
