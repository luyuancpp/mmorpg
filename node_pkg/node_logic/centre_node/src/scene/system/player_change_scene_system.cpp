#include "player_change_scene_system.h"

#include "pbc/common_error_tip.pb.h"
#include "pbc/scene_error_tip.pb.h"
#include "scene/system/scene_system.h"
#include "thread_local/storage.h"
#include "muduo/base/Logging.h"
#include "proto/logic/event/scene_event.pb.h"
#include "time/system/time_system.h"


// 添加切换场景信息到队列
uint32_t PlayerChangeSceneUtil::PushChangeSceneInfo(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo) {
	auto& changeSceneQueue = tls.registry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	changeSceneQueue.enqueue(changeInfo);
	changeSceneQueue.front()->set_change_time(TimeUtil::NowSecondsUTC());
	return kSuccess;
}

// 移除队列中首个切换场景信息
void PlayerChangeSceneUtil::PopFrontChangeSceneQueue(entt::entity player) {
	auto& changeSceneQueue = tls.registry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	changeSceneQueue.dequeue();
}

// 设置当前切换场景信息的切换状态
void PlayerChangeSceneUtil::SetChangeSceneNodeStatus(entt::entity player, ChangeSceneInfoPBComponent::eChangeGsStatus s) {
	auto& changeSceneQueue = tls.registry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	if (changeSceneQueue.empty()) {
		return;
	}
	changeSceneQueue.front()->set_change_gs_status(s);
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
	auto& getChangeSceneQueue = tls.registry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	auto& changeSceneQueue = getChangeSceneQueue;
	if (changeSceneQueue.empty()) {
		return;
	}
	const auto changeInfo = changeSceneQueue.front();
	if (changeInfo->change_gs_type() == ChangeSceneInfoPBComponent::eSameGs) {
		ProcessSameGsChangeScene(player, *changeInfo);
	}
	else if (changeInfo->change_gs_type() == ChangeSceneInfoPBComponent::eDifferentGs) {
		ProcessDifferentGsChangeScene(player, *changeInfo);
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


//ChangeSceneQueueComponent
//└─ std::deque<ChangeSceneTask>    ← 可读写的任务队列
//
//ChangeSceneTask
//├─ 原始PB结构（ChangeSceneInfoPB）
//├─ 当前状态（ChangeState枚举）
//├─ 时间戳
//└─ 回调指针或上下文信息（可选）
//
//PlayerChangeSceneUtil::Tick()
//└─ 对每个玩家调用 AdvanceSceneChangeState(player)
//
//AdvanceSceneChangeState(entt::entity)
//└─ 根据状态决定做什么：
//- ePendingLeave → LeaveScene() → 状态转 eLeaving
//- eLeaving → 等通知回调（或继续轮询）
//- eLeaveComplete → 通知目标GS
//- eEnterOK → 调用 EnterScene()
//- eDone → PopFront，进入下一个任务

//
//ePendingLeave
//↓ （调用 LeaveScene）
//eLeaving
//↓ （存储完成回调）
//eWaitingEnter
//↓ （目标节点确认已接收并处理完毕）
//eEnterGsSceneSucceed
//↓ （调用 EnterScene）
//eGateEnterGsSceneSucceed
//↓
//DONE(PopFront)



// 确认玩家成功进入场景后的操作
void PlayerChangeSceneUtil::OnEnterSceneOk(entt::entity player) {
	S2CEnterScene ev;
	ev.set_entity(entt::to_integral(player));
	tls.dispatcher.trigger(ev);
}

void PlayerChangeSceneUtil::AdvanceSceneChangeState(entt::entity player) {
	auto& queue = tls.registry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	if (queue.empty()) return;

	auto& task = *queue.front();

	switch (task.state()) {
	case ChangeSceneInfoPBComponent::ePendingLeave:
		if (task.change_gs_type() == ChangeSceneInfoPBComponent::eSameGs) {
			auto destScene = entt::entity{ task.guid() };
			if (destScene == entt::null) {
				queue.dequeue();
				return;
			}

			SceneUtil::LeaveScene({ player });
			SceneUtil::EnterScene({ destScene, player });
			queue.dequeue();
			OnEnterSceneOk(player);
		}
		else {
			SceneUtil::LeaveScene({ player });

			/*SimulateAsyncStore(player, [&]() {
				task.state = ChangeSceneState::WaitingEnter;
				LOG_INFO << "[SceneChange] Store complete for Player " << entt::to_integral(player);
				});*/

			task.set_state(ChangeSceneInfoPBComponent::eLeaving);
		}
		break;
	case ChangeSceneInfoPBComponent::eLeaving:
		// 等待中心节点收到【离开成功的通知】，再调用 SetState(player, eWaitingEnter);
		break;
	case ChangeSceneInfoPBComponent::eWaitingEnter:
		// 等待中心服务器或目标节点 RPC 通知我们切换完成
		break;

	case ChangeSceneInfoPBComponent::eEnterSucceed: {
		auto destScene = entt::entity{ task.guid() };
		if (destScene == entt::null) {
			SceneUtil::EnterDefaultScene({ player });
		}
		else {
			SceneUtil::EnterScene({ destScene, player });
		}
		task.set_state(ChangeSceneInfoPBComponent::eGateEnterSucceed);
		break;
	}

	case ChangeSceneInfoPBComponent::eGateEnterSucceed:
		queue.dequeue();
		OnEnterSceneOk(player);
		break;

	default:
		break;
	}
}
