﻿#include "mission_system.h"
#include <ranges>
#include "error_handling/error_handling.h"
#include "muduo/base/Logging.h"
#include "condition_config.h"
#include "mission/constants/mission_constants.h"
#include "mission/comp/mission_comp.h"
#include "thread_local/storage.h"
#include "macros/return_define.h"
#include "pbc/mission_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "proto/logic/component/mission_comp.pb.h"
#include "proto/logic/event/mission_event.pb.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace {

	// Comparison functions for mission conditions
	std::array<std::function<bool(uint32_t, uint32_t)>, 5> condition_comparison_functions = {
		{
			[](const uint32_t actual_value, const uint32_t config_value) { return actual_value >= config_value; },
			[](const uint32_t actual_value, const uint32_t config_value) { return actual_value > config_value; },
			[](const uint32_t actual_value, const uint32_t config_value) { return actual_value <= config_value; },
			[](const uint32_t actual_value, const uint32_t config_value) { return actual_value < config_value; },
			[](const uint32_t actual_value, const uint32_t config_value) { return actual_value == config_value; }
		}
	};

} // anonymous namespace

// Function to retrieve reward for completing a mission
uint32_t MissionSystem::GetMissionReward(const GetRewardParam& param) {
	// Check if player exists in the registry
	if (!tls.registry.valid(param.playerId)) {
		LOG_ERROR << "Player not found: playerId = " << tls.registry.get<Guid>(param.playerId);
		return PrintStackAndReturnError(kInvalidParameter);
	}

	// Retrieve mission reward component for the player
	auto* const missionRewardComp = tls.registry.try_get<RewardListPBComponent>(param.playerId);
	if (nullptr == missionRewardComp) {
		LOG_ERROR << "Mission reward component not found: playerId = " << tls.registry.get<Guid>(param.playerId);
		return PrintStackAndReturnError(kPlayerMissionComponentNotFound);
	}

	// Check if the mission ID is valid for reward
	auto rewardMissionIdMap = missionRewardComp->mutable_can_reward_mission_id();
	if (rewardMissionIdMap->find(param.missionId) == rewardMissionIdMap->end()) {
		LOG_ERROR << "Mission ID not found in reward list: missionId = " << param.missionId << ", playerId = " << tls.registry.get<Guid>(param.playerId);
		return PrintStackAndReturnError(kMissionIdNotInRewardList);
	}

	// Remove mission ID from reward list
	rewardMissionIdMap->erase(param.missionId);
	LOG_INFO << "Removed mission ID from reward list: missionId = " << param.missionId << ", playerId = " << tls.registry.get<Guid>(param.playerId);
	return kSuccess;
}

// Function to check conditions before accepting a mission
uint32_t MissionSystem::CheckMissionAcceptance(const AcceptMissionEvent& acceptEvent, MissionsComponent* missionComp) {
	// Check if mission is unaccepted and uncompleted
	RETURN_ON_ERROR(missionComp->IsMissionUnaccepted(acceptEvent.mission_id()));
	RETURN_ON_ERROR(missionComp->IsMissionUncompleted(acceptEvent.mission_id()));

	// Ensure mission configuration is valid
	RETURN_IF_TRUE(!missionComp->GetMissionConfig()->HasKey(acceptEvent.mission_id()), kInvalidTableId);

	// Retrieve mission sub-type and type
	auto missionSubType = missionComp->GetMissionConfig()->GetMissionSubType(acceptEvent.mission_id());
	auto missionType = missionComp->GetMissionConfig()->GetMissionType(acceptEvent.mission_id());

	// If mission type should not repeat, check type filter
	if (missionComp->IsMissionTypeNotRepeated()) {
		auto missionTypeSubTypePair = std::make_pair(missionType, missionSubType);
		RETURN_IF_TRUE(missionComp->GetTypeFilter().find(missionTypeSubTypePair) != missionComp->GetTypeFilter().end(), kMissionTypeAlreadyExists);
	}

	return kSuccess;
}

// Function to accept a mission
uint32_t MissionSystem::AcceptMission(const AcceptMissionEvent& acceptEvent) {
	// Convert entity ID to player entity
	const entt::entity playerEntity = entt::to_entity(acceptEvent.entity());

	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComponent>(playerEntity);
	if (nullptr == missionComp) {
		LOG_ERROR << "Missions component not found for playerEntity = " << tls.registry.get<Guid>(playerEntity);
		return kPlayerMissionComponentNotFound;
	}

	// Check acceptance conditions
	auto ret = CheckMissionAcceptance(acceptEvent, missionComp);
	if (ret != kSuccess) {
		LOG_ERROR << "CheckMissionAcceptance failed for mission_id = " << acceptEvent.mission_id()
			<< ", playerEntity = " << tls.registry.get<Guid>(playerEntity);
		return ret;
	}

	// Retrieve mission sub-type and type
	auto missionSubType = missionComp->GetMissionConfig()->GetMissionSubType(acceptEvent.mission_id());
	auto missionType = missionComp->GetMissionConfig()->GetMissionType(acceptEvent.mission_id());

	// Add mission type filter if type should not repeat
	if (missionComp->IsMissionTypeNotRepeated()) {
		auto missionTypeSubTypePair = std::make_pair(missionType, missionSubType);
		missionComp->GetTypeFilter().emplace(missionTypeSubTypePair);
	}

	// Create mission protobuf component
	MissionPBComponent missionPb;
	missionPb.set_id(acceptEvent.mission_id());

	// Initialize mission progress based on conditions
	for (const auto& conditionId : missionComp->GetMissionConfig()->GetConditionIds(acceptEvent.mission_id())) {
		FetchConditionTableOrContinue(conditionId);
		
		missionPb.add_progress(0);
		missionComp->GetEventMissionsClassify()[conditionTable->condition_type()].emplace(acceptEvent.mission_id());
	}

	// Insert mission into missions component
	missionComp->GetMissionsComp().mutable_missions()->insert({ acceptEvent.mission_id(), std::move(missionPb) });

	// Dispatch event for mission acceptance
	{
		OnAcceptedMissionEvent onAcceptedMissionEvent;
		onAcceptedMissionEvent.set_entity(entt::to_integral(playerEntity));
		onAcceptedMissionEvent.set_mission_id(acceptEvent.mission_id());
		tls.dispatcher.enqueue(onAcceptedMissionEvent);
		LOG_INFO << "Mission accepted for playerEntity = " << tls.registry.get<Guid>(playerEntity) << ", mission_id = " << acceptEvent.mission_id();
	}

	return kSuccess;
}

// Function to abandon a mission
uint32_t MissionSystem::AbandonMission(const AbandonParam& param) {
	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComponent>(param.playerId);
	if (nullptr == missionComp) {
		LOG_ERROR << "Missions component not found for playerId = " << tls.registry.get<Guid>(param.playerId);
		return kPlayerMissionComponentNotFound;
	}

	// Check if mission is uncompleted
	if (kMissionAlreadyCompleted == missionComp->IsMissionUncompleted(param.missionId)) {
		LOG_ERROR << "Mission is already completed for playerId = " << tls.registry.get<Guid>(param.playerId) << ", missionId = " << param.missionId;
		return kMissionAlreadyCompleted;
	}

	// Remove mission ID from reward list if applicable
	auto* const missionReward = tls.registry.try_get<RewardListPBComponent>(param.playerId);
	if (nullptr != missionReward) {
		missionReward->mutable_can_reward_mission_id()->erase(param.missionId);
	}

	// Remove mission from missions component
	missionComp->GetMissionsComp().mutable_missions()->erase(param.missionId);
	missionComp->AbandonMission(param.missionId);
	missionComp->GetMissionsComp().mutable_mission_begin_time()->erase(param.missionId);

	// Delete mission classification
	DeleteMissionClassification(param.playerId, param.missionId);
	LOG_INFO << "Mission abandoned for playerId = " << tls.registry.get<Guid>(param.playerId) << ", missionId = " << param.missionId;
	return kSuccess;
}

// Function to complete all missions for a player
void MissionSystem::CompleteAllMissions(entt::entity playerEntity, uint32_t operation) {
	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComponent>(playerEntity);
	if (nullptr == missionComp) {
		return;
	}

	// Mark all missions as complete
	for (const auto& missionId : missionComp->GetMissionsComp().missions() | std::views::keys) {
		if (!MissionBitMap.contains(missionId)){
			continue;
		}
		missionComp->GetCompleteMissions().set(MissionBitMap.at(missionId), true);
	}

	// Clear all missions
	missionComp->GetMissionsComp().mutable_missions()->clear();
}

// Function to check if a condition is completed
bool IsConditionFulfilled(uint32_t conditionId, uint32_t progressValue) {
	FetchConditionTableOrReturnFalse(conditionId);
	return condition_comparison_functions[static_cast<size_t>(conditionTable->comparison())](progressValue, conditionTable->amount());
}


// Check if all conditions of a mission are fulfilled
bool MissionSystem::AreAllConditionsFulfilled(const MissionPBComponent& mission, uint32_t missionId, MissionsComponent* missionComp) {
	// Retrieve mission conditions from configuration
	const auto& conditions = missionComp->GetMissionConfig()->GetConditionIds(missionId);

	// Ensure progress matches condition requirements
	for (int32_t i = 0; i < mission.progress_size() && i < conditions.size(); ++i) {
		FetchConditionTableOrContinue(conditions.at(i));

		// Check if condition is fulfilled
		if (!IsConditionFulfilled(conditionTable->id(), mission.progress(i))) {
			return false;
		}
	}

	return true;
}
// Process mission condition events
void MissionSystem::HandleMissionConditionEvent(const MissionConditionEvent& conditionEvent) {
	// Ignore if no conditions are provided
	if (conditionEvent.condtion_ids().empty()) {
		LOG_ERROR << "HandleMissionConditionEvent: Empty condition IDs for entity = " << conditionEvent.entity();
		return;
	}

	// Convert entity ID to player entity
	const entt::entity playerEntity = entt::to_entity(conditionEvent.entity());

	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComponent>(playerEntity);
	if (nullptr == missionComp) {
		LOG_ERROR << "HandleMissionConditionEvent: Missions component not found for playerEntity = " << tls.registry.get<Guid>(playerEntity);
		return;
	}

	// Find relevant missions based on condition type
	auto classifyMissionsIt = missionComp->GetEventMissionsClassify().find(conditionEvent.condition_type());
	if (classifyMissionsIt == missionComp->GetEventMissionsClassify().end()) {
		LOG_ERROR << "HandleMissionConditionEvent: No missions found for condition type = " << conditionEvent.condition_type()
			<< " for playerEntity = " << tls.registry.get<Guid>(playerEntity);
		return;
	}

	// Track completed missions
	std::unordered_set<uint32_t> completedMissionsThisTime;

	// Iterate through classified missions
	for (auto& missionId : classifyMissionsIt->second) {
		auto missionIter = missionComp->GetMissionsComp().mutable_missions()->find(missionId);
		if (missionIter == missionComp->GetMissionsComp().mutable_missions()->end()) {
			continue;
		}
		auto& mission = missionIter->second;

		// Update mission progress
		if (!UpdateMissionProgress(conditionEvent, mission)) {
			continue;
		}

		// Check if all conditions are now fulfilled
		if (!AreAllConditionsFulfilled(mission, missionId, missionComp)) {
			continue;
		}

		mission.set_status(MissionPBComponent::E_MISSION_COMPLETE);
		completedMissionsThisTime.emplace(missionId);
		missionComp->GetMissionsComp().mutable_missions()->erase(missionIter);
	}

	// Process completion events for completed missions
	OnMissionCompletion(playerEntity, completedMissionsThisTime);
}

// Remove mission classification
void MissionSystem::RemoveMissionClassification(MissionsComponent* missionComp, uint32_t missionId) {
	// Retrieve conditions from mission configuration
	const auto& configConditions = missionComp->GetMissionConfig()->GetConditionIds(missionId);

	// Remove mission classification based on condition type
	for (int32_t i = 0; i < configConditions.size(); ++i) {
		FetchConditionTableOrContinue(configConditions.Get(i));
		missionComp->GetEventMissionsClassify()[conditionTable->condition_type()].erase(missionId);
	}
}

// Delete mission classification
void MissionSystem::DeleteMissionClassification(entt::entity playerEntity, uint32_t missionId) {
	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComponent>(playerEntity);
	if (nullptr == missionComp) {
		return;
	}

	// Remove mission classification
	RemoveMissionClassification(missionComp, missionId);

	// Remove mission type filter if applicable
	auto missionSubType = missionComp->GetMissionConfig()->GetMissionSubType(missionId);
	if (missionSubType > 0 && missionComp->IsMissionTypeNotRepeated()) {
		auto missionTypeSubTypePair = std::make_pair(missionComp->GetMissionConfig()->GetMissionType(missionId), missionSubType);
		missionComp->GetTypeFilter().erase(missionTypeSubTypePair);
	}
}

// Update mission progress based on event
bool MissionSystem::UpdateMissionProgress(const MissionConditionEvent& conditionEvent, MissionPBComponent& mission) {
	// Ignore if no conditions are provided
	if (conditionEvent.condtion_ids().empty()) {
		return false;
	}

	// Retrieve mission component for the player
	const auto* const missionComp = tls.registry.try_get<MissionsComponent>(entt::to_entity(conditionEvent.entity()));
	if (nullptr == missionComp) {
		return false;
	}

	bool missionUpdated = false;

	// Retrieve mission conditions from configuration
	const auto& missionConditions = missionComp->GetMissionConfig()->GetConditionIds(mission.id());

	// Iterate through mission conditions and update progress
	for (int32_t i = 0; i < mission.progress_size() && i < missionConditions.size(); ++i) {
		FetchConditionTableOrContinue(missionConditions.at(i));
	
		// Update progress if condition matches event
		if (UpdateProgressIfConditionMatches(conditionEvent, mission, i, conditionTable)) {
			missionUpdated = true;
		}
	}

	// Update mission status based on progress
	if (missionUpdated) {
		UpdateMissionStatus(mission, missionConditions);
	}

	return missionUpdated;
}

// Update mission progress if conditions match the event
bool MissionSystem::UpdateProgressIfConditionMatches(const MissionConditionEvent& conditionEvent, MissionPBComponent& mission, int index, const ConditionTable* conditionRow) {
	// Retrieve old progress value
	const auto oldProgress = mission.progress(index);

	// Check if condition is already completed
	if (IsConditionFulfilled(conditionRow->id(), oldProgress)) {
		return false;
	}

	// Check if condition type matches event type
	if (conditionEvent.condition_type() != conditionRow->condition_type()) {
		return false;
	}

	// Count matching conditions
	size_t configConditionCount = 0;
	size_t matchConditionCount = 0;

	auto countMatchingConditions = [&matchConditionCount, &conditionEvent, &configConditionCount](const auto& configConditions, size_t index) {
		if (!configConditions.empty()) {
			++configConditionCount;
		}
		if (index >= conditionEvent.condtion_ids().size()) {
			return;
		}
		auto eventConditionId = conditionEvent.condtion_ids(index);
		// Check if any condition in the table's columns matches any of the event's conditions
		for (int32_t ci = 0; ci < configConditions.size(); ++ci) {
			if (configConditions.Get(ci) != eventConditionId) {
				continue;
			}
			// Found a match in this column
			++matchConditionCount;
			break;
		}
		};

	// Count matching conditions for up to four condition slots
	countMatchingConditions(conditionRow->condition1(), 0);
	countMatchingConditions(conditionRow->condition2(), 1);
	countMatchingConditions(conditionRow->condition3(), 2);
	countMatchingConditions(conditionRow->condition4(), 3);

	// Ensure all expected conditions match
	if (configConditionCount > 0 && matchConditionCount != configConditionCount) {
		return false;
	}

	// Update mission progress
	mission.set_progress(index, conditionEvent.amount() + oldProgress);
	return true;
}


// Update mission status based on progress
void MissionSystem::UpdateMissionStatus(MissionPBComponent& mission, const google::protobuf::RepeatedField<uint32_t>& missionConditions) {
	// Iterate through mission conditions and update progress
	for (int32_t i = 0; i < mission.progress_size() && i < missionConditions.size(); ++i) {
		FetchConditionTableOrContinue(missionConditions.at(i));
	
		auto newProgress = mission.progress(i);

		// Clamp progress to condition amount if exceeded
		if (!IsConditionFulfilled(conditionTable->id(), newProgress)) {
			continue;
		}

		mission.set_progress(i, std::min(newProgress, conditionTable->amount()));
	}
}

// Process completion events for completed missions
void MissionSystem::OnMissionCompletion(entt::entity playerEntity, const std::unordered_set<uint32_t>& completedMissionsThisTime) {
	// Ignore if no missions are completed
	if (completedMissionsThisTime.empty()) {
		return;
	}

	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComponent>(playerEntity);
	if (nullptr == missionComp) {
		return;
	}

	// Process each completed mission
	for (const auto& missionId : completedMissionsThisTime) {
		// Delete mission classification
		DeleteMissionClassification(playerEntity, missionId);

		// Retrieve mission reward component for the player
		auto* const missionReward = tls.registry.try_get<RewardListPBComponent>(playerEntity);

		// Create mission condition event
		MissionConditionEvent missionConditionEvent;
		missionConditionEvent.set_entity(entt::to_integral(playerEntity));
		missionConditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionCompleteMission));
		missionConditionEvent.set_amount(1);

		// Process mission completion rewards and events
		for (const auto& missionId : completedMissionsThisTime) {
			// Mark mission as complete

			if (!MissionBitMap.contains(missionId)){
				continue;
			}
			
			missionComp->GetCompleteMissions().set(MissionBitMap.at(missionId), true);

			// Check if mission has rewards and should be automatically rewarded
			if (missionComp->GetMissionConfig()->GetRewardId(missionId) > 0 && missionComp->GetMissionConfig()->AutoReward(missionId)) {
				// Enqueue mission award event
				OnMissionAwardEvent missionAwardEvent;
				missionAwardEvent.set_entity(entt::to_integral(playerEntity));
				missionAwardEvent.set_mission_id(missionId);
				tls.dispatcher.enqueue(missionAwardEvent);
			}
			else if (nullptr != missionReward && missionComp->GetMissionConfig()->GetRewardId(missionId) > 0) {
				// Mark mission as rewardable
				missionReward->mutable_can_reward_mission_id()->insert({ missionId, false });
			}

			// Create mission acceptance event for next missions
			AcceptMissionEvent acceptMissionEvent;
			acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
			const auto& nextMissions = missionComp->GetMissionConfig()->GetNextmissionTableIds(missionId);

			// Enqueue acceptance event for each next mission
			for (int32_t i = 0; i < nextMissions.size(); ++i) {
				acceptMissionEvent.set_mission_id(nextMissions.Get(i));
				tls.dispatcher.enqueue(acceptMissionEvent);
			}

			// Add mission ID to condition event for next processing
			missionConditionEvent.clear_condtion_ids();
			missionConditionEvent.mutable_condtion_ids()->Add(missionId);
			tls.dispatcher.enqueue(missionConditionEvent);
		}
	}
}