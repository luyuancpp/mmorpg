#include "player_change_scene.h"

#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/scene_error_tip.pb.h"
#include "scene/system/scene.h"

#include "muduo/base/Logging.h"
#include "proto/common/event/scene_event.pb.h"
#include "time/system/time.h"
#include <engine/thread_context/registry_manager.h>
#include <engine/thread_context/dispatcher_manager.h>
#include <modules/scene/system/scene_common.h>


uint32_t PlayerChangeSceneUtil::PushChangeSceneInfo(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo) {
	auto& changeSceneQueue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	changeSceneQueue.enqueue(changeInfo);
	changeSceneQueue.front()->set_change_time(TimeSystem::NowSecondsUTC());
	return kSuccess;
}

void PlayerChangeSceneUtil::PopFrontChangeSceneQueue(entt::entity player) {
	auto& changeSceneQueue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	changeSceneQueue.dequeue();
}

void PlayerChangeSceneUtil::SetCurrentChangeSceneState(entt::entity player, ChangeSceneInfoPBComponent::eChangeSceneState s) {
	auto& changeSceneQueue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	if (changeSceneQueue.empty()) {
		return;
	}
	changeSceneQueue.front()->set_state(s);
}

void PlayerChangeSceneUtil::CopySceneInfoToChangeInfo(ChangeSceneInfoPBComponent& changeInfo, const SceneInfoPBComponent& sceneInfo) {
	changeInfo.set_scene_confid(sceneInfo.scene_confid());
	changeInfo.set_dungen_confid(sceneInfo.dungen_confid());
	changeInfo.set_guid(sceneInfo.guid());
	changeInfo.set_mirror_confid(sceneInfo.mirror_confid());
}


// Scene-change state machine
//
// ChangeSceneQueueComponent
// └─ std::deque<ChangeSceneTask>    (read/write task queue)
//
// ChangeSceneTask
// ├─ ChangeSceneInfoPB (protobuf payload)
// ├─ Current state (ChangeState enum)
// ├─ Timestamp
// └─ Callback / context (optional)
//
// State transitions:
//   ePendingLeave  → LeaveScene() [called by centre]
//   eLeaving       → leave-complete callback [from GS to centre]
//   eWaitingEnter  → target GS ready notification [from target GS to centre]
//   eEnterSucceed  → EnterScene() [called by centre]
//   eGateEnterSucceed → notify client (OnEnterSceneOk) → dequeue


// Callback from target scene node or gate to centre
void PlayerChangeSceneUtil::OnTargetSceneNodeEnterComplete(entt::entity player) {
	auto& queue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
	if (queue.empty()) return;

	auto& task = *queue.front();
	if (task.state() == ChangeSceneInfoPBComponent::eEnterSucceed) {
		task.set_state(ChangeSceneInfoPBComponent::eGateEnterSucceed);
		LOG_INFO << "[SceneChange] Player " << entt::to_integral(player) << " fully entered target scene";
	}
}


void PlayerChangeSceneUtil::OnEnterSceneOk(entt::entity player) {
	S2CEnterScene ev;
	ev.set_entity(entt::to_integral(player));
	dispatcher.trigger(ev);
}

void PlayerChangeSceneUtil::ProgressSceneChangeState(entt::entity player) {
	auto& queue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);
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

			SceneCommon::LeaveScene({ player });
			SceneCommon::EnterScene({ destScene, player });
			queue.dequeue();
			OnEnterSceneOk(player);
		}
		else {
			SceneCommon::LeaveScene({ player });
			task.set_state(ChangeSceneInfoPBComponent::eLeaving);
		}
		break;
	case ChangeSceneInfoPBComponent::eLeaving:
		// Waiting for leave-complete callback before transitioning to eWaitingEnter.
		// TODO: No timeout handling yet — could get stuck in eLeaving (no callback)
		// or eWaitingEnter (target GS never confirms ready).
		break;
	case ChangeSceneInfoPBComponent::eWaitingEnter:
		// Waiting for centre or target node RPC to confirm transition
		break;

	case ChangeSceneInfoPBComponent::eEnterSucceed: {
		auto destScene = entt::entity{ task.guid() };
		if (destScene == entt::null) {
			SceneSystem::EnterDefaultScene({ player });
		}
		else {
			SceneCommon::EnterScene({ destScene, player });
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

