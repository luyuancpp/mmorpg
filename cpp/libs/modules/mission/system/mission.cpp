#include "mission.h"

#include <ranges>
#include <unordered_set>

#include "muduo/base/Logging.h"
#include <thread_context/dispatcher_manager.h>

#include "engine/core/error_handling/error_handling.h"
#include "engine/core/macros/return_define.h"
#include "engine/core/macros/error_return.h"
#include "engine/core/utils/bit_index/bit_index_util.h"

#include "condition/condition_util.h"
#include "mission/comp/mission_comp.h"
#include "mission/constants/mission.h"

#include "table/code/condition_table.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/mission_error_tip.pb.h"

#include "proto/common/component/mission_comp.pb.h"
#include "proto/common/event/mission_event.pb.h"

uint32_t MissionSystem::GetMissionReward(const GetRewardParam &param, MissionsComp &missionComp)
{
	if (!tlsEcs.actorRegistry.valid(param.playerEntity))
	{
		LOG_ERROR << "Player not found: playerId = " << tlsEcs.actorRegistry.get<Guid>(param.playerEntity);
		return PrintStackAndReturnError(kInvalidParameter);
	}

	const auto playerId = tlsEcs.actorRegistry.get_or_emplace<Guid>(param.playerEntity);

	if (!missionComp.IsClaimable(param.missionId))
	{
		LOG_ERROR << "Mission not claimable: missionId = " << param.missionId << ", playerId = " << playerId;
		return PrintStackAndReturnError(kMissionIdNotInRewardList);
	}

	SetBit(MissionBitMap, missionComp.GetClaimableRewards(), param.missionId, false);
	LOG_INFO << "Reward claimed: missionId = " << param.missionId << ", playerId = " << playerId;
	return kSuccess;
}

uint32_t MissionSystem::CheckMissionAcceptance(const AcceptMissionEvent &acceptEvent, MissionsComp &missionComp, const IMissionConfig &config)
{
	RETURN_ON_ERROR(missionComp.ValidateNotAccepted(acceptEvent.mission_id()));
	RETURN_ON_ERROR(missionComp.ValidateNotCompleted(acceptEvent.mission_id()));
	RETURN_IF_TRUE(!config.HasKey(acceptEvent.mission_id()), kInvalidTableId);

	if (missionComp.IsMissionTypeNotRepeated())
	{
		auto typeKey = std::make_pair(config.GetMissionType(acceptEvent.mission_id()),
									  config.GetMissionSubType(acceptEvent.mission_id()));
		RETURN_IF_TRUE(missionComp.GetTypeFilter().count(typeKey) > 0, kMissionTypeAlreadyExists);
	}

	return kSuccess;
}

uint32_t MissionSystem::AcceptMission(const AcceptMissionEvent &acceptEvent, MissionsComp &missionComp, const IMissionConfig &config)
{
	const entt::entity playerEntity = entt::to_entity(acceptEvent.entity());
	const uint32_t missionId = acceptEvent.mission_id();

	auto ret = CheckMissionAcceptance(acceptEvent, missionComp, config);
	if (ret != kSuccess)
	{
		LOG_ERROR << "CheckMissionAcceptance failed: missionId = " << missionId
				  << ", playerId = " << tlsEcs.actorRegistry.get_or_emplace<Guid>(playerEntity);
		return ret;
	}

	// Register type filter to prevent duplicate type acceptance
	if (missionComp.IsMissionTypeNotRepeated())
	{
		auto typeKey = std::make_pair(config.GetMissionType(missionId), config.GetMissionSubType(missionId));
		missionComp.GetMutableTypeFilter().emplace(typeKey);
	}

	MissionComp missionPb;
	missionPb.set_id(missionId);

	for (const auto &conditionId : config.GetConditionIds(missionId))
	{
		LookupConditionOrContinue(conditionId);
		missionPb.add_progress(0);
		missionComp.GetMutableEventMissionsClassify()[conditionRow->condition_category()].emplace(missionId);
	}

	missionComp.GetMutableMissionList().mutable_missions()->insert({missionId, std::move(missionPb)});

	OnAcceptedMissionEvent onAcceptedMissionEvent;
	onAcceptedMissionEvent.set_entity(entt::to_integral(playerEntity));
	onAcceptedMissionEvent.set_mission_id(missionId);
	tlsEcs.dispatcher.trigger(onAcceptedMissionEvent);
	LOG_INFO << "Mission accepted: missionId = " << missionId
			 << ", playerId = " << tlsEcs.actorRegistry.get_or_emplace<Guid>(playerEntity);

	return kSuccess;
}

uint32_t MissionSystem::AbandonMission(const AbandonParam &param, MissionsComp &comp, const IMissionConfig &config)
{
	if (kMissionAlreadyCompleted == comp.ValidateNotCompleted(param.missionId))
	{
		return MAKE_ERROR_MSG(kMissionAlreadyCompleted,
							  "missionId=" << param.missionId
										   << " playerId=" << tlsEcs.actorRegistry.get_or_emplace<Guid>(param.playerEntity));
	}

	SetBit(MissionBitMap, comp.GetClaimableRewards(), param.missionId, false);
	comp.GetMutableMissionList().mutable_missions()->erase(param.missionId);
	comp.AbandonMission(param.missionId);
	comp.GetMutableMissionList().mutable_mission_begin_time()->erase(param.missionId);

	DeleteMissionClassification(comp, param.missionId, config);
	LOG_INFO << "Mission abandoned: missionId = " << param.missionId
			 << ", playerId = " << tlsEcs.actorRegistry.get_or_emplace<Guid>(param.playerEntity);
	return kSuccess;
}

void MissionSystem::CompleteAllMissions(entt::entity playerEntity, uint32_t operation, MissionsComp &comp)
{
	for (const auto &missionId : comp.GetMissionList().missions() | std::views::keys)
	{
		SetBit(MissionBitMap, comp.GetMutableCompletedMissions(), missionId);
	}
	comp.GetMutableMissionList().mutable_missions()->clear();
}

bool MissionSystem::AreAllConditionsFulfilled(const MissionComp &mission, uint32_t missionId, MissionsComp &missionComp, const IMissionConfig &config)
{
	const auto &conditions = config.GetConditionIds(missionId);
	const auto &targetCounts = config.GetTargetCounts(missionId);
	for (int32_t i = 0; i < mission.progress_size() && i < conditions.size(); ++i)
	{
		const uint32_t tc = (i < targetCounts.size()) ? targetCounts.at(i) : 0;
		if (!condition_util::IsFulfilled(conditions.at(i), mission.progress(i), tc))
		{
			return false;
		}
	}
	return true;
}
void MissionSystem::HandleConditionEvent(const ConditionEvent &conditionEvent, MissionsComp &comp, const IMissionConfig &config)
{
	if (conditionEvent.condition_ids().empty())
	{
		LOG_ERROR << "HandleConditionEvent: empty condition IDs for entity = " << conditionEvent.entity();
		return;
	}

	const entt::entity playerEntity = entt::to_entity(conditionEvent.entity());

	auto classifyIt = comp.GetMutableEventMissionsClassify().find(conditionEvent.condition_type());
	if (classifyIt == comp.GetMutableEventMissionsClassify().end())
	{
		return;
	}

	std::unordered_set<uint32_t> completedThisRound;

	for (auto &missionId : classifyIt->second)
	{
		auto missionIter = comp.GetMutableMissionList().mutable_missions()->find(missionId);
		if (missionIter == comp.GetMutableMissionList().mutable_missions()->end())
		{
			continue;
		}
		auto &mission = missionIter->second;

		if (!UpdateMissionProgress(conditionEvent, mission, config))
		{
			continue;
		}
		if (!AreAllConditionsFulfilled(mission, missionId, comp, config))
		{
			continue;
		}

		mission.set_status(MissionComp::E_MISSION_COMPLETE);
		completedThisRound.emplace(missionId);
		comp.GetMutableMissionList().mutable_missions()->erase(missionIter);
	}

	OnMissionCompletion(playerEntity, completedThisRound, comp, config);
}

void MissionSystem::RemoveMissionClassification(MissionsComp &missionComp, uint32_t missionId, const IMissionConfig &config)
{
	for (const auto &conditionId : config.GetConditionIds(missionId))
	{
		LookupConditionOrContinue(conditionId);
		missionComp.GetMutableEventMissionsClassify()[conditionRow->condition_category()].erase(missionId);
	}
}

void MissionSystem::DeleteMissionClassification(MissionsComp &missionComp, uint32_t missionId, const IMissionConfig &config)
{
	RemoveMissionClassification(missionComp, missionId, config);

	auto missionSubType = config.GetMissionSubType(missionId);
	if (missionSubType > 0 && missionComp.IsMissionTypeNotRepeated())
	{
		missionComp.GetMutableTypeFilter().erase(
			std::make_pair(config.GetMissionType(missionId), missionSubType));
	}
}

bool MissionSystem::UpdateMissionProgress(const ConditionEvent &conditionEvent, MissionComp &mission, const IMissionConfig &config)
{
	if (conditionEvent.condition_ids().empty())
	{
		return false;
	}

	bool updated = false;
	const auto &missionConditions = config.GetConditionIds(mission.id());
	const auto &targetCounts = config.GetTargetCounts(mission.id());

	for (int32_t i = 0; i < mission.progress_size() && i < missionConditions.size(); ++i)
	{
		LookupConditionOrContinue(missionConditions.at(i));
		const uint32_t tc = (i < targetCounts.size()) ? targetCounts.at(i) : 0;
		if (UpdateProgressIfConditionMatches(conditionEvent, mission, i, conditionRow, tc))
		{
			updated = true;
		}
	}

	if (updated)
	{
		UpdateMissionStatus(mission, missionConditions, targetCounts);
	}
	return updated;
}

bool MissionSystem::UpdateProgressIfConditionMatches(const ConditionEvent &conditionEvent, MissionComp &mission, int index, const ConditionTable *conditionTable, uint32_t targetCount)
{
	const auto oldProgress = mission.progress(index);
	if (condition_util::IsFulfilled(conditionTable->id(), oldProgress, targetCount))
	{
		return false;
	}
	if (conditionEvent.condition_type() != conditionTable->condition_category())
	{
		return false;
	}
	if (!condition_util::MatchesEventSlots(conditionTable, conditionEvent.condition_ids()))
	{
		return false;
	}

	mission.set_progress(index, conditionEvent.amount() + oldProgress);
	return true;
}

void MissionSystem::UpdateMissionStatus(MissionComp &mission, const google::protobuf::RepeatedField<uint32_t> &missionConditions, const google::protobuf::RepeatedField<uint32_t> &targetCounts)
{
	for (int32_t i = 0; i < mission.progress_size() && i < missionConditions.size(); ++i)
	{
		const uint32_t tc = (i < targetCounts.size()) ? targetCounts.at(i) : 0;
		mission.set_progress(i, condition_util::ClampIfFulfilled(missionConditions.at(i), mission.progress(i), tc));
	}
}

void MissionSystem::OnMissionCompletion(entt::entity playerEntity, const std::unordered_set<uint32_t> &completedMissions, MissionsComp &missionComp, const IMissionConfig &config)
{
	if (completedMissions.empty())
	{
		return;
	}

	for (const auto &missionId : completedMissions)
	{
		DeleteMissionClassification(missionComp, missionId, config);
		SetBit(MissionBitMap, missionComp.GetMutableCompletedMissions(), missionId);

		switch (GetRewardAction(config, missionId))
		{
		case RewardAction::kAutoGrant:
		{
			OnMissionAwardEvent awardEvent;
			awardEvent.set_entity(entt::to_integral(playerEntity));
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

		// Chain: enqueue next missions in the sequence
		AcceptMissionEvent acceptEvent;
		acceptEvent.set_entity(entt::to_integral(playerEntity));
		for (const auto &nextId : config.GetNextMissionTableIds(missionId))
		{
			acceptEvent.set_mission_id(nextId);
			tlsEcs.dispatcher.enqueue(acceptEvent);
		}

		// Notify condition system so "complete mission X" conditions can trigger
		ConditionEvent conditionEvent;
		conditionEvent.set_entity(entt::to_integral(playerEntity));
		conditionEvent.set_condition_type(static_cast<uint32_t>(eConditionType::kConditionCompleteMission));
		conditionEvent.set_amount(1);
		conditionEvent.mutable_condition_ids()->Add(missionId);
		tlsEcs.dispatcher.enqueue(conditionEvent);
	}
}
