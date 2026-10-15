#include "mission.h"

#include <algorithm>
#include <ranges>
#include <limits>
#include <unordered_set>
#include <vector>

#include "muduo/base/Logging.h"
#include "engine/core/error_handling/error_handling.h"
#include "engine/core/macros/return_define.h"
#include "engine/core/macros/error_return.h"
#include "engine/core/utils/bit_index/bit_index_util.h"

#include "condition/condition_util.h"
#include "mission/comp/mission_comp.h"

#include "table/code/condition_table.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/mission_error_tip.pb.h"
#include "proto/common/component/mission_comp.pb.h"
#include "proto/common/event/mission_event.pb.h"
#include <condition/condition_type.h>

uint32_t MissionSystem::GetMissionReward(const GetRewardParam &param, MissionsComp &missionComp)
{
	// Validate the entity before touching its components (get<Guid> asserts existence).
	if (!tlsEcs.actorRegistry.valid(param.playerEntity))
	{
		LOG_ERROR << "Claim reward failed: invalid player entity = " << entt::to_integral(param.playerEntity);
		return PrintStackAndReturnError(kInvalidParameter);
	}

	const auto* owner = tlsEcs.actorRegistry.try_get<Guid>(param.playerEntity);
    if (owner == nullptr) return kInvalidParameter;
    const auto playerId = *owner;

	if (!missionComp.IsClaimable(param.missionId))
	{
		LOG_ERROR << "Claim reward failed: mission not claimable, missionId = " << param.missionId
				  << ", playerId = " << playerId;
		return PrintStackAndReturnError(kMissionIdNotInRewardList);
	}

	// 仅提交领取状态；生产发奖必须走 PlayerMissionSystem::ClaimReward。
	missionComp.ClearClaimable(param.missionId);
	LOG_INFO << "Reward claimed: missionId = " << param.missionId << ", playerId = " << playerId;
	return kSuccess;
}

uint32_t MissionSystem::CheckMissionAcceptance(const AcceptMissionEvent &acceptEvent, MissionsComp &missionComp, const IMissionConfig &config)
{
	const uint32_t missionId = acceptEvent.mission_id();

	RETURN_ON_ERROR(missionComp.ValidateNotAccepted(missionId));
	RETURN_ON_ERROR(missionComp.ValidateNotCompleted(missionId));
	RETURN_IF_TRUE(!config.HasKey(missionId) || !MissionBitMap.contains(missionId), kInvalidTableId);
    for (const auto conditionId : config.GetConditionIds(missionId))
    {
        const auto [condition, error] = ConditionTableManager::Instance().FindByIdSilent(conditionId);
        RETURN_IF_TRUE(condition == nullptr, kInvalidTableData);
    }

	// When a (sub)type may only be active once, reject a second mission of that type.
	if (missionComp.IsMissionTypeNotRepeated())
	{
		const auto typeKey = std::make_pair(config.GetMissionType(missionId), config.GetMissionSubType(missionId));
		RETURN_IF_TRUE(missionComp.GetTypeFilter().count(typeKey) > 0, kMissionTypeAlreadyExists);
	}

	return kSuccess;
}

uint32_t MissionSystem::AcceptMission(const AcceptMissionEvent &acceptEvent, MissionsComp &missionComp, const IMissionConfig &config)
{
	const entt::entity playerEntity = entt::to_entity(acceptEvent.entity());
	const uint32_t missionId = acceptEvent.mission_id();

	if (const uint32_t checkResult = CheckMissionAcceptance(acceptEvent, missionComp, config); checkResult != kSuccess)
	{
		LOG_ERROR << "Accept mission rejected: missionId = " << missionId
				  << ", playerId = " << entt::to_integral(playerEntity);
		return checkResult;
	}

	// Reserve this (sub)type so a duplicate of the same type cannot be accepted later.
	if (missionComp.IsMissionTypeNotRepeated())
	{
		const auto typeKey = std::make_pair(config.GetMissionType(missionId), config.GetMissionSubType(missionId));
		missionComp.GetMutableTypeFilter().emplace(typeKey);
	}

	// Create the runtime mission with one zero-initialised progress slot per condition,
	// and index it by condition category so condition events can locate it quickly.
	MissionComp mission;
	mission.set_id(missionId);
	for (const auto &conditionId : config.GetConditionIds(missionId))
	{
		LookupConditionOrContinue(conditionId);
		mission.add_progress(0);
		missionComp.GetMutableEventMissionsClassify()[conditionRow->condition_category()].emplace(missionId);
	}
	missionComp.GetMutableMissionList().mutable_missions()->insert({missionId, std::move(mission)});

	OnAcceptedMissionEvent acceptedEvent;
	acceptedEvent.set_entity(entt::to_integral(playerEntity));
	acceptedEvent.set_mission_id(missionId);
	tlsEcs.dispatcher.trigger(acceptedEvent);

	LOG_INFO << "Mission accepted: missionId = " << missionId
			 << ", playerId = " << entt::to_integral(playerEntity);
	return kSuccess;
}

uint32_t MissionSystem::AbandonMission(const AbandonParam &param, MissionsComp &missionComp, const IMissionConfig &config)
{
	if (kMissionAlreadyCompleted == missionComp.ValidateNotCompleted(param.missionId))
	{
		return MAKE_ERROR_MSG(kMissionAlreadyCompleted,
							  "missionId=" << param.missionId
										   << " playerId=" << entt::to_integral(param.playerEntity));
	}

	missionComp.ClearClaimable(param.missionId);
	missionComp.GetMutableMissionList().mutable_missions()->erase(param.missionId);
	missionComp.AbandonMission(param.missionId);
	missionComp.GetMutableMissionList().mutable_mission_begin_time()->erase(param.missionId);
	UnregisterMissionIndexes(missionComp, param.missionId, config);

	LOG_INFO << "Mission abandoned: missionId = " << param.missionId
			 << ", playerId = " << entt::to_integral(param.playerEntity);
	return kSuccess;
}

// CompleteAllMissions — Bulk-completion helper (GM / debug / admin tools).
//
// Marks every currently-accepted mission as completed in the player's
// completed-bitmap, then clears the active mission list. It does NOT:
//   - grant rewards or set claimable bits
//   - enqueue the next mission in a chain
//   - fire the kConditionCompleteMission event
//
// Intended for GM "clear all my quests" / test-harness scenarios where you
// want the bitmap state without side effects.
//
// For the normal in-game "this mission's conditions are now met" path,
// use OnMissionCompletion instead (called from HandleConditionEvent).
// The distinction is intentional — do NOT collapse the two functions.
// See todo.md #225 for the historical lesson.
void MissionSystem::CompleteAllMissions(entt::entity /*playerEntity*/, MissionsComp &missionComp)
{
	for (const auto &missionId : missionComp.GetMissionList().missions() | std::views::keys)
	{
		SetBit(MissionBitMap, missionComp.GetMutableCompletedMissions(), missionId);
	}
	missionComp.GetMutableMissionList().mutable_missions()->clear();
}

bool MissionSystem::AreAllConditionsFulfilled(const MissionComp &mission, uint32_t missionId, const IMissionConfig &config)
{
	const auto &conditionIds = config.GetConditionIds(missionId);
	const auto &targetCounts = config.GetTargetCounts(missionId);

    // 少目标的旧/损坏进度不能因为只遍历已有槽而提前完成。
    if (conditionIds.empty() || mission.progress_size() != conditionIds.size()) return false;
	const int32_t slotCount = mission.progress_size();
	for (int32_t i = 0; i < slotCount; ++i)
	{
		const uint32_t targetCount = (i < targetCounts.size()) ? targetCounts.at(i) : 0;
		if (!condition_util::IsFulfilled(conditionIds.at(i), mission.progress(i), targetCount))
		{
			return false;
		}
	}
	return true;
}

void MissionSystem::HandleConditionEvent(const ConditionEvent &conditionEvent, MissionsComp &missionComp, const IMissionConfig &config, uint32_t onlyMissionId)
{
	if (conditionEvent.condition_ids().empty())
	{
		LOG_ERROR << "HandleConditionEvent: empty condition IDs for entity = " << conditionEvent.entity();
		return;
	}

	// Only missions that watch this condition category can be affected by the event.
	auto &missionsByCondition = missionComp.GetMutableEventMissionsClassify();
	const auto watchersIt = missionsByCondition.find(conditionEvent.condition_type());
	if (watchersIt == missionsByCondition.end())
	{
		return;
	}

	const entt::entity playerEntity = entt::to_entity(conditionEvent.entity());
	auto &missions = *missionComp.GetMutableMissionList().mutable_missions();

	std::unordered_set<uint32_t> justCompleted;
	for (const uint32_t missionId : watchersIt->second)
	{
        if (onlyMissionId != 0 && missionId != onlyMissionId) continue;
		const auto missionIt = missions.find(missionId);
		if (missionIt == missions.end())
		{
			continue;
		}

		MissionComp &mission = missionIt->second;
        if (mission.id() != missionId || !config.HasKey(missionId) || missionComp.IsComplete(missionId) ||
            mission.status() == MissionComp::E_MISSION_TIME_OUT || mission.status() == MissionComp::E_MISSION_FAILD)
            continue;
		if (!UpdateMissionProgress(conditionEvent, mission, config))
		{
			continue;
		}
		if (!AreAllConditionsFulfilled(mission, missionId, config))
		{
			continue;
		}

		mission.set_status(MissionComp::E_MISSION_COMPLETE);
		justCompleted.emplace(missionId);
		missions.erase(missionIt);
	}

	OnMissionCompletion(playerEntity, justCompleted, missionComp, config);
}

void MissionSystem::RemoveMissionFromConditionIndex(MissionsComp &missionComp, uint32_t missionId, const IMissionConfig &config)
{
	for (const auto &conditionId : config.GetConditionIds(missionId))
	{
		LookupConditionOrContinue(conditionId);
		missionComp.GetMutableEventMissionsClassify()[conditionRow->condition_category()].erase(missionId);
	}
}

void MissionSystem::UnregisterMissionIndexes(MissionsComp &missionComp, uint32_t missionId, const IMissionConfig &config)
{
	RemoveMissionFromConditionIndex(missionComp, missionId, config);

	const uint32_t missionSubType = config.GetMissionSubType(missionId);
	if (missionComp.IsMissionTypeNotRepeated())
	{
		missionComp.GetMutableTypeFilter().erase(
			std::make_pair(config.GetMissionType(missionId), missionSubType));
	}
}

bool MissionSystem::UpdateMissionProgress(const ConditionEvent &conditionEvent, MissionComp &mission, const IMissionConfig &config)
{
	// A condition whose table defines no slot filters matches *any* event, so an
	// event carrying no condition ids must not be allowed to advance progress.
	if (conditionEvent.condition_ids().empty())
	{
		return false;
	}

	const auto &conditionIds = config.GetConditionIds(mission.id());
	const auto &targetCounts = config.GetTargetCounts(mission.id());

    if (mission.progress_size() != conditionIds.size()) return false;
	const int32_t slotCount = mission.progress_size();
    const bool ordered = config.IsConditionOrdered(mission.id());
	bool progressed = false;
	for (int32_t i = 0; i < slotCount; ++i)
	{
		LookupConditionOrContinue(conditionIds.at(i));
		const uint32_t targetCount = (i < targetCounts.size()) ? targetCounts.at(i) : 0;
        if (ordered && condition_util::IsFulfilled(conditionIds.at(i), mission.progress(i), targetCount)) continue;
		if (UpdateProgressIfConditionMatches(conditionEvent, mission, i, conditionRow, targetCount))
		{
			progressed = true;
		}
        // 顺序目标一条事实最多推进当前一格，不能同一击杀跨过重复的后续目标。
        if (ordered) break;
	}
	return progressed;
}

bool MissionSystem::UpdateProgressIfConditionMatches(const ConditionEvent &conditionEvent, MissionComp &mission, int index, const ConditionTable *conditionTable, uint32_t targetCount)
{
	const auto currentProgress = mission.progress(index);

	// Already satisfied -- nothing left to add for this slot.
	if (condition_util::IsFulfilled(conditionTable->id(), currentProgress, targetCount))
	{
		return false;
	}
	// The event must match this condition's category and its slot filters.
	if (conditionEvent.condition_type() != conditionTable->condition_category())
	{
		return false;
	}
    if (conditionTable->condition_category() == static_cast<uint32_t>(eConditionType::kConditionLevelUp))
    {
        // condition1 是等级门槛：已有 20 级的玩家接取“达到 10 级”也应满足。
        if (!conditionTable->condition1().empty() && std::none_of(
            conditionTable->condition1().begin(), conditionTable->condition1().end(),
            [&conditionEvent](uint32_t level) { return conditionEvent.amount() >= level; })) return false;
        if (!conditionTable->condition2().empty() || !conditionTable->condition3().empty() || !conditionTable->condition4().empty())
            return false;
    }
    else if (!condition_util::MatchesEventSlots(conditionTable, conditionEvent.condition_ids()))
    {
        return false;
    }

    const uint64_t target = targetCount > 0 ? targetCount : conditionTable->target_count();
    const bool owned = conditionTable->quantity_type() == 1 ||
        conditionTable->condition_category() == static_cast<uint32_t>(eConditionType::kConditionLevelUp);
    const uint64_t candidate = owned ? conditionEvent.amount() :
        uint64_t{currentProgress} + conditionEvent.amount();
    // 累计目标饱和；严格 > 的完成值是 target+1，不能夹回 target 后又变成未完成。
    const uint64_t ceiling = conditionTable->comparison_op() == 1 ? target + 1 : target;
    const auto newProgress = static_cast<uint32_t>(std::min<uint64_t>(
        owned ? candidate : std::min(candidate, ceiling), std::numeric_limits<uint32_t>::max()));
    if (newProgress == currentProgress) return false;
    mission.set_progress(index, newProgress);
    return true;
}

// OnMissionCompletion — Per-mission completion handler for the normal
// gameplay path. Called from HandleConditionEvent after each progress
// update, with the set of missions that just transitioned to COMPLETE
// in this round.
//
// For each completed mission this runs the full side-effect chain:
//   1. Remove the mission from condition→missions classification index
//      and set the completed bit in the player's bitmap.
//   2. Reward handling per IMissionConfig::GetRewardAction:
//        kAutoGrant  → OnMissionAwardEvent (claimable bit kept until the grant succeeds)
//        kClaimable  → set the claimable-reward bit (player pulls later)
//   3. Chain follow-up: AcceptMissionEvent for every mission
//      listed in GetNextMissionTableIds, so quest chains progress.
//   4. Fire ConditionEvent(kConditionCompleteMission) so other systems
//      whose conditions depend on "completed mission X" can advance.
// 第 2~4 步的事件在本轮所有任务状态落定之后同步 trigger(原因见函数内注释)。
//
// Contrast with CompleteAllMissions above: that one is GM-only and skips
// the entire side-effect chain. Do not merge the two. See todo.md #225.
void MissionSystem::OnMissionCompletion(entt::entity playerEntity, const std::unordered_set<uint32_t> &completedMissions, MissionsComp &missionComp, const IMissionConfig &config)
{
	// 必须保留完整 64 位:高 32 位是 version,截成 uint32 后槽位复用过的玩家
	// 还原出来的是 version 0 的句柄,handler 里 valid() 判否,事件被静默丢弃。
	const uint64_t playerEntityValue = entt::to_integral(playerEntity);

	// 副作用事件先收集、等全部完成任务的状态落定后再派发。
	// 不能边处理边派发:同一批里的任务 B 已从任务表摘掉但还没置完成位,
	// 此时任务 A 的后续接取若恰好是 B,会把 B 重新接一遍。
	// 也不能用 dispatcher.enqueue:生产代码里没有任何地方调用 update(),事件会永远
	// 堆在队列里;而即便补上 update(),entt 3.13 的 publish() 把 events[pos] 的引用
	// 交给监听者,监听者再往同类型队列 enqueue(ConditionEvent 完成任务会再产生
	// ConditionEvent)时 push_back 扩容,这个引用就悬空了。trigger 按值拷贝事件,没有这个问题。
	std::vector<OnMissionAwardEvent> awardEvents;
	std::vector<AcceptMissionEvent> acceptEvents;
	std::vector<ConditionEvent> conditionEvents;

	for (const uint32_t missionId : completedMissions)
	{
		UnregisterMissionIndexes(missionComp, missionId, config);
		SetBit(MissionBitMap, missionComp.GetMutableCompletedMissions(), missionId);

		// Deliver or arm the reward according to the mission's reward policy.
		switch (GetRewardAction(config, missionId))
		{
		case RewardAction::kAutoGrant:
		{
            // 先保留待领取权，再尝试发奖；满包、禁发或号段不足都可以重试。
            missionComp.RestoreClaimable(missionId);
			OnMissionAwardEvent &awardEvent = awardEvents.emplace_back();
			awardEvent.set_entity(playerEntityValue);
			awardEvent.set_mission_id(missionId);
			break;
		}
		case RewardAction::kClaimable:
			SetBit(MissionBitMap, missionComp.GetClaimableRewards(), missionId);
			break;
		default:
			break;
		}

		// Auto-accept the follow-up missions so the quest chain advances.
		for (const uint32_t nextMissionId : config.GetNextMissionTableIds(missionId))
		{
			AcceptMissionEvent &nextMissionEvent = acceptEvents.emplace_back();
			nextMissionEvent.set_entity(playerEntityValue);
			nextMissionEvent.set_mission_id(nextMissionId);
		}

		// Notify the condition system so "complete mission X" conditions can trigger.
		ConditionEvent &completeMissionCondition = conditionEvents.emplace_back();
		completeMissionCondition.set_entity(playerEntityValue);
		completeMissionCondition.set_condition_type(static_cast<uint32_t>(eConditionType::kConditionCompleteMission));
		completeMissionCondition.set_amount(1);
		completeMissionCondition.mutable_condition_ids()->Add(missionId);
	}

	// 派发顺序与原先按类型 update 的口径一致:先发奖,再接后续任务,最后推进「完成任务 X」条件。
	// 监听者可能重入本系统(接取 → 补算进度 → 又完成任务),此时本函数的循环已经结束,
	// 局部向量不会再被修改,重入是安全的;链路深度受任务配置约束,有限。
	for (OnMissionAwardEvent &event : awardEvents)
	{
		tlsEcs.dispatcher.trigger(std::move(event));
	}
	for (AcceptMissionEvent &event : acceptEvents)
	{
		tlsEcs.dispatcher.trigger(std::move(event));
	}
	for (ConditionEvent &event : conditionEvents)
	{
		tlsEcs.dispatcher.trigger(std::move(event));
	}
}
