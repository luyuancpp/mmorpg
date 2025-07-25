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
	auto& changeSceneQueue = tls.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	changeSceneQueue.enqueue(changeInfo);
	changeSceneQueue.front()->set_change_time(TimeUtil::NowSecondsUTC());
	return kSuccess;
}

// 移除队列中首个切换场景信息
void PlayerChangeSceneUtil::PopFrontChangeSceneQueue(entt::entity player) {
	auto& changeSceneQueue = tls.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	changeSceneQueue.dequeue();
}

// 设置当前切换场景信息的切换状态
void PlayerChangeSceneUtil::SetCurrentChangeSceneState(entt::entity player, ChangeSceneInfoPBComponent::eChangeSceneState s) {
	auto& changeSceneQueue = tls.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	if (changeSceneQueue.empty()) {
		return;
	}
	changeSceneQueue.front()->set_state(s);
}

// 将场景信息复制到切换场景信息中
void PlayerChangeSceneUtil::CopySceneInfoToChangeInfo(ChangeSceneInfoPBComponent& changeInfo, const SceneInfoPBComponent& sceneInfo) {
	changeInfo.set_scene_confid(sceneInfo.scene_confid());
	changeInfo.set_dungen_confid(sceneInfo.dungen_confid());
	changeInfo.set_guid(sceneInfo.guid());
	changeInfo.set_mirror_confid(sceneInfo.mirror_confid());
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

//ePendingLeave
//↓ LeaveScene()[由中心调用]
//	eLeaving
//		↓ 离开完成回调[由 GS 发回中心]
//		eWaitingEnter
//		↓ 目标服加载完成通知[由目标GS通知中心]
//		eEnterSucceed
//			↓ 玩家实体进入场景[由中心调用 EnterScene()]
//			eGateEnterSucceed
//			↓ 玩家进入成功回调[由目标GS 或 Gate 发回中心]
//			✅ 发客户端通知（OnEnterSceneOk）

//
//ePendingLeave
//↓ 调用 LeaveScene()（如果目标是其他 GS）
//eLeaving
//↓ 收到 GS 离开完成回调 → 设置为 eWaitingEnter
//eWaitingEnter
//↓ 目标 GS 加载完 → 设置为 eEnterSucceed
//eEnterSucceed
//↓ 调用 EnterScene() → 玩家实体加载到目标场景
//eGateEnterSucceed
//↓ 通知客户端场景进入成功 → 出队


// 目标GS或Gate回调中心
void PlayerChangeSceneUtil::OnTargetSceneNodeEnterComplete(entt::entity player) {
	auto& queue = tls.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	if (queue.empty()) return;

	auto& task = *queue.front();
	if (task.state() == ChangeSceneInfoPBComponent::eEnterSucceed) {
		task.set_state(ChangeSceneInfoPBComponent::eGateEnterSucceed);
		LOG_INFO << "[SceneChange] Player " << entt::to_integral(player) << " fully entered target scene";
	}
}


// 确认玩家成功进入场景后的操作
void PlayerChangeSceneUtil::OnEnterSceneOk(entt::entity player) {
	S2CEnterScene ev;
	ev.set_entity(entt::to_integral(player));
	tls.dispatcher.trigger(ev);
}

void PlayerChangeSceneUtil::ProgressSceneChangeState(entt::entity player) {
	auto& queue = tls.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
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
			task.set_state(ChangeSceneInfoPBComponent::eLeaving);
		}
		break;
	case ChangeSceneInfoPBComponent::eLeaving:
		// 等待中心节点收到【离开成功的通知】，再调用 SetState(player, eWaitingEnter);
		//当前的系统没有处理因网络/RPC超时卡在某个状态的逻辑，例如：
		//todo 
		//❗ 可能卡住的状态：
		//	eLeaving：玩家调用 LeaveScene() 后，迟迟未收到回调。

		//	eWaitingEnter：目标GS没有及时回复“我加载好了”。
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
