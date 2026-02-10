#include "player_change_room.h"

#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/scene_error_tip.pb.h"
#include "scene/system/room.h"

#include "muduo/base/Logging.h"
#include "proto/common/event/scene_event.pb.h"
#include "time/system/time.h"
#include <engine/threading/registry_manager.h>
#include <engine/threading/dispatcher_manager.h>
#include <modules/scene/system/room_common.h>


// 添加切换场景信息到队列
uint32_t PlayerChangeRoomUtil::PushChangeSceneInfo(entt::entity player, const ChangeRoomInfoPBComponent& changeInfo) {
	auto& changeSceneQueue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	changeSceneQueue.enqueue(changeInfo);
	changeSceneQueue.front()->set_change_time(TimeSystem::NowSecondsUTC());
	return kSuccess;
}

// 移除队列中首个切换场景信息
void PlayerChangeRoomUtil::PopFrontChangeSceneQueue(entt::entity player) {
	auto& changeSceneQueue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	changeSceneQueue.dequeue();
}

// 设置当前切换场景信息的切换状态
void PlayerChangeRoomUtil::SetCurrentChangeSceneState(entt::entity player, ChangeRoomInfoPBComponent::eChangeSceneState s) {
	auto& changeSceneQueue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	if (changeSceneQueue.empty()) {
		return;
	}
	changeSceneQueue.front()->set_state(s);
}

// 将场景信息复制到切换场景信息中
void PlayerChangeRoomUtil::CopySceneInfoToChangeInfo(ChangeRoomInfoPBComponent& changeInfo, const RoomInfoPBComponent& sceneInfo) {
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
void PlayerChangeRoomUtil::OnTargetSceneNodeEnterComplete(entt::entity player) {
	auto& queue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	if (queue.empty()) return;

	auto& task = *queue.front();
	if (task.state() == ChangeRoomInfoPBComponent::eEnterSucceed) {
		task.set_state(ChangeRoomInfoPBComponent::eGateEnterSucceed);
		LOG_INFO << "[SceneChange] Player " << entt::to_integral(player) << " fully entered target scene";
	}
}


// 确认玩家成功进入场景后的操作
void PlayerChangeRoomUtil::OnEnterRoomOk(entt::entity player) {
	S2CEnterRoom ev;
	ev.set_entity(entt::to_integral(player));
	dispatcher.trigger(ev);
}

void PlayerChangeRoomUtil::ProgressSceneChangeState(entt::entity player) {
	auto& queue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	if (queue.empty()) return;

	auto& task = *queue.front();

	switch (task.state()) {
	case ChangeRoomInfoPBComponent::ePendingLeave:
		if (task.change_gs_type() == ChangeRoomInfoPBComponent::eSameGs) {
			auto destScene = entt::entity{ task.guid() };
			if (destScene == entt::null) {
				queue.dequeue();
				return;
			}

			RoomCommon::LeaveRoom({ player });
			RoomCommon::EnterRoom({ destScene, player });
			queue.dequeue();
			OnEnterRoomOk(player);
		}
		else {
			RoomCommon::LeaveRoom({ player });
			task.set_state(ChangeRoomInfoPBComponent::eLeaving);
		}
		break;
	case ChangeRoomInfoPBComponent::eLeaving:
		// 等待中心节点收到【离开成功的通知】，再调用 SetState(player, eWaitingEnter);
		//当前的系统没有处理因网络/RPC超时卡在某个状态的逻辑，例如：
		//todo 
		//❗ 可能卡住的状态：
		//	eLeaving：玩家调用 LeaveScene() 后，迟迟未收到回调。

		//	eWaitingEnter：目标GS没有及时回复“我加载好了”。
		break;
	case ChangeRoomInfoPBComponent::eWaitingEnter:
		// 等待中心服务器或目标节点 RPC 通知我们切换完成
		break;

	case ChangeRoomInfoPBComponent::eEnterSucceed: {
		auto destScene = entt::entity{ task.guid() };
		if (destScene == entt::null) {
			RoomSystem::EnterDefaultRoom({ player });
		}
		else {
			RoomCommon::EnterRoom({ destScene, player });
		}
		break;
	}

	case ChangeRoomInfoPBComponent::eGateEnterSucceed:
		queue.dequeue();
		OnEnterRoomOk(player);
		break;

	default:
		break;
	}
}
