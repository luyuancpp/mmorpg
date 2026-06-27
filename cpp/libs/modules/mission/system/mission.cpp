#include "mission.h"

#include <algorithm>
#include <ranges>
#include <unordered_set>

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

	const auto playerId = tlsEcs.actorRegistry.get<Guid>(param.playerEntity);

	if (!missionComp.IsClaimable(param.missionId))
	{
		LOG_ERROR << "Claim reward failed: mission not claimable, missionId = " << param.missionId
				  << ", playerId = " << playerId;
		return PrintStackAndReturnError(kMissionIdNotInRewardList);
	}

	// Clear the claimable bit so the same reward cannot be claimed twice.
	SetBit(MissionBitMap, missionComp.GetClaimableRewards(), param.missionId, false);
	LOG_INFO << "Reward claimed: missionId = " << param.missionId << ", playerId = " << playerId;
	return kSuccess;
}

uint32_t MissionSystem::CheckMissionAcceptance(const AcceptMissionEvent &acceptEvent, MissionsComp &missionComp, const IMissionConfig &config)
{
	const uint32_t missionId = acceptEvent.mission_id();

	RETURN_ON_ERROR(missionComp.ValidateNotAccepted(missionId));
	RETURN_ON_ERROR(missionComp.ValidateNotCompleted(missionId));
	RETURN_IF_TRUE(!config.HasKey(missionId), kInvalidTableId);

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

	SetBit(MissionBitMap, missionComp.GetClaimableRewards(), param.missionId, false);
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

	const int32_t slotCount = std::min<int32_t>(mission.progress_size(), conditionIds.size());
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

void MissionSystem::HandleConditionEvent(const ConditionEvent &conditionEvent, MissionsComp &missionComp, const IMissionConfig &config)
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
		const auto missionIt = missions.find(missionId);
		if (missionIt == missions.end())
		{
			continue;
		}

		MissionComp &mission = missionIt->second;
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
	if (missionSubType > 0 && missionComp.IsMissionTypeNotRepeated())
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

	const int32_t slotCount = std::min<int32_t>(mission.progress_size(), conditionIds.size());
	bool progressed = false;
	for (int32_t i = 0; i < slotCount; ++i)
	{
		LookupConditionOrContinue(conditionIds.at(i));
		const uint32_t targetCount = (i < targetCounts.size()) ? targetCounts.at(i) : 0;
		if (UpdateProgressIfConditionMatches(conditionEvent, mission, i, conditionRow, targetCount))
		{
			progressed = true;
		}
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
	if (!condition_util::MatchesEventSlots(conditionTable, conditionEvent.condition_ids()))
	{
		return false;
	}

	// Accumulate progress, then clamp to the target so it never overshoots.
	const uint32_t newProgress = currentProgress + conditionEvent.amount();
	mission.set_progress(index, condition_util::ClampIfFulfilled(conditionTable->id(), newProgress, targetCount));
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
//        kAutoGrant  → enqueue OnMissionAwardEvent (delivered immediately)
//        kClaimable  → set the claimable-reward bit (player pulls later)
//   3. Chain follow-up: enqueue AcceptMissionEvent for every mission
//      listed in GetNextMissionTableIds, so quest chains progress.
//   4. Fire ConditionEvent(kConditionCompleteMission) so other systems
//      whose conditions depend on "completed mission X" can advance.
//
// Contrast with CompleteAllMissions above: that one is GM-only and skips
// the entire side-effect chain. Do not merge the two. See todo.md #225.
void MissionSystem::OnMissionCompletion(entt::entity playerEntity, const std::unordered_set<uint32_t> &completedMissions, MissionsComp &missionComp, const IMissionConfig &config)
{
	const uint32_t playerId = entt::to_integral(playerEntity);

	for (const uint32_t missionId : completedMissions)
	{
		UnregisterMissionIndexes(missionComp, missionId, config);
		SetBit(MissionBitMap, missionComp.GetMutableCompletedMissions(), missionId);

		// Deliver or arm the reward according to the mission's reward policy.
		switch (GetRewardAction(config, missionId))
		{
		case RewardAction::kAutoGrant:
		{
			OnMissionAwardEvent awardEvent;
			awardEvent.set_entity(playerId);
			awardEvent.set_mission_id(missionId);
			tlsEcs.dispatcher.enqueue(awardEvent);
			break;
		}
		case RewardAction::kClaimable:
			SetBit(MissionBitMap, missionComp.GetClaimableRewards(), missionId);
			break;
		default:
			break;
		}

		// Auto-accept the follow-up missions so the quest chain advances.
		AcceptMissionEvent nextMissionEvent;
		nextMissionEvent.set_entity(playerId);
		for (const uint32_t nextMissionId : config.GetNextMissionTableIds(missionId))
		{
			nextMissionEvent.set_mission_id(nextMissionId);
			tlsEcs.dispatcher.enqueue(nextMissionEvent);
		}

		// Notify the condition system so "complete mission X" conditions can trigger.
		ConditionEvent completeMissionCondition;
		completeMissionCondition.set_entity(playerId);
		completeMissionCondition.set_condition_type(static_cast<uint32_t>(eConditionType::kConditionCompleteMission));
		completeMissionCondition.set_amount(1);
		completeMissionCondition.mutable_condition_ids()->Add(missionId);
		tlsEcs.dispatcher.enqueue(completeMissionCondition);
	}
}
