﻿#include "mission_system.h"
#include <ranges>
#include "muduo/base/Logging.h"
#include "condition_config.h"
#include "constants/mission.h"
#include "comp/mission.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"
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

// Get reward for completing a mission
uint32_t MissionSystem::GetMissionReward(const GetRewardParam& param) {
	// Check if player exists in the registry
	if (!tls.registry.valid(param.playerId)) {
		LOG_ERROR << "player not found";
		return kInvalidParameter;
	}

	// Retrieve mission reward component for the player
	auto* const missionRewardComp = tls.registry.try_get<MissionRewardPbComp>(param.playerId);
	if (nullptr == missionRewardComp) {
		return kPlayerMissionComponentNotFound;
	}

	// Check if the mission ID is valid for reward
	auto rewardMissionIdMap = missionRewardComp->mutable_can_reward_mission_id();
	if (rewardMissionIdMap->find(param.missionId) == rewardMissionIdMap->end()) {
		return kMissionIdNotInRewardList;
	}

	// Remove mission ID from reward list
	rewardMissionIdMap->erase(param.missionId);
	return kOK;
}

// Check conditions before accepting a mission
uint32_t MissionSystem::CheckMissionAcceptance(const AcceptMissionEvent& acceptEvent, MissionsComp* missionComp) {
	// Check if mission is unaccepted and uncompleted
	RET_CHECK_RETURN(missionComp->IsMissionUnaccepted(acceptEvent.mission_id()));
	RET_CHECK_RETURN(missionComp->IsMissionUncompleted(acceptEvent.mission_id()));

	// Ensure mission configuration is valid
	CHECK_CONDITION(!missionComp->GetMissionConfig()->HasKey(acceptEvent.mission_id()), kTableIdInvalid);

	// Retrieve mission sub-type and type
	auto missionSubType = missionComp->GetMissionConfig()->GetMissionSubType(acceptEvent.mission_id());
	auto missionType = missionComp->GetMissionConfig()->GetMissionType(acceptEvent.mission_id());

	// If mission type should not repeat, check type filter
	if (missionComp->IsMissionTypeNotRepeated()) {
		auto missionTypeSubTypePair = std::make_pair(missionType, missionSubType);
		CHECK_CONDITION(missionComp->GetTypeFilter().find(missionTypeSubTypePair) != missionComp->GetTypeFilter().end(), kMissionTypeRepeated);
	}

	return kOK;
}

// Accept a mission
uint32_t MissionSystem::AcceptMission(const AcceptMissionEvent& acceptEvent) {
	// Convert entity ID to player entity
	const entt::entity playerEntity = entt::to_entity(acceptEvent.entity());

	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComp>(playerEntity);
	if (nullptr == missionComp) {
		return kPlayerMissionComponentNotFound;
	}

	// Check acceptance conditions
	auto ret = CheckMissionAcceptance(acceptEvent, missionComp);
	if (ret != kOK) {
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
	MissionPbComp missionPb;
	missionPb.set_id(acceptEvent.mission_id());

	// Initialize mission progress based on conditions
	for (const auto& conditionId : missionComp->GetMissionConfig()->GetConditionIds(acceptEvent.mission_id())) {
		const auto* const conditionConfig = condition_config::GetSingleton().get(conditionId);
		if (nullptr == conditionConfig) {
			LOG_ERROR << "condition not found: " << conditionId;
			continue;
		}
		missionPb.add_progress(0);
		missionComp->GetEventMissionsClassify()[conditionConfig->condition_type()].emplace(acceptEvent.mission_id());
	}

	// Insert mission into missions component
	missionComp->GetMissionsComp().mutable_missions()->insert({ acceptEvent.mission_id(), std::move(missionPb) });

	// Dispatch event for mission acceptance
	{
		OnAcceptedMissionEvent onAcceptedMissionEvent;
		onAcceptedMissionEvent.set_entity(entt::to_integral(playerEntity));
		onAcceptedMissionEvent.set_mission_id(acceptEvent.mission_id());
		tls.dispatcher.enqueue(onAcceptedMissionEvent);
	}

	return kOK;
}

// Abandon a mission
uint32_t MissionSystem::AbandonMission(const AbandonParam& param) {
	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComp>(param.playerId);
	if (nullptr == missionComp) {
		return kPlayerMissionComponentNotFound;
	}

	// Check if mission is uncompleted
	RET_CHECK_RETURN(missionComp->IsMissionUncompleted(param.missionId));

	// Remove mission ID from reward list if applicable
	auto* const missionReward = tls.registry.try_get<MissionRewardPbComp>(param.playerId);
	if (nullptr != missionReward) {
		missionReward->mutable_can_reward_mission_id()->erase(param.missionId);
	}

	// Remove mission from missions component
	missionComp->GetMissionsComp().mutable_missions()->erase(param.missionId);
	missionComp->GetMissionsComp().mutable_complete_missions()->erase(param.missionId);
	missionComp->GetMissionsComp().mutable_mission_begin_time()->erase(param.missionId);

	// Delete mission classification
	DeleteMissionClassification(param.playerId, param.missionId);
	return kOK;
}

// Complete all missions for a player
void MissionSystem::CompleteAllMissions(entt::entity playerEntity, uint32_t operation) {
	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComp>(playerEntity);
	if (nullptr == missionComp) {
		return;
	}

	// Mark all missions as complete
	for (const auto& missionId : missionComp->GetMissionsComp().missions() | std::views::keys) {
		missionComp->GetMissionsComp().mutable_complete_missions()->insert({ missionId, false });
	}

	// Clear all missions
	missionComp->GetMissionsComp().mutable_missions()->clear();
}

// Check if a condition is completed
bool IsConditionFulfilled(uint32_t conditionId, const uint32_t progressValue) {
	const auto* conditionRow = condition_config::GetSingleton().get(conditionId);
	if (nullptr == conditionRow) {
		return false;
	}

	// Perform comparison using predefined function
	auto operation = conditionRow->operation();
	if (operation >= condition_comparison_functions.size()) {
		operation = 0; // Default to the first comparison function
	}
	return condition_comparison_functions[operation](progressValue, conditionRow->amount());
}

// Check if all conditions of a mission are fulfilled
bool MissionSystem::AreAllConditionsFulfilled(const MissionPbComp& mission, uint32_t missionId, MissionsComp* missionComp) {
	// Retrieve mission conditions from configuration
	const auto& conditions = missionComp->GetMissionConfig()->GetConditionIds(missionId);

	// Ensure progress matches condition requirements
	for (int32_t i = 0; i < mission.progress_size() && i < conditions.size(); ++i) {
		const auto* const conditionRow = condition_config::GetSingleton().get(conditions.at(i));
		if (nullptr == conditionRow) {
			continue;
		}

		// Check if condition is fulfilled
		if (!IsConditionFulfilled(conditionRow->id(), mission.progress(i))) {
			return false;
		}
	}

	return true;
}


// Process mission condition events
void MissionSystem::HandleMissionConditionEvent(const MissionConditionEvent& conditionEvent) {
	// Ignore if no conditions are provided
	if (conditionEvent.condtion_ids().empty()) {
		return;
	}

	// Convert entity ID to player entity
	const entt::entity playerEntity = entt::to_entity(conditionEvent.entity());

	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComp>(playerEntity);
	if (nullptr == missionComp) {
		return;
	}

	// Find relevant missions based on condition type
	auto classifyMissionsIt = missionComp->GetEventMissionsClassify().find(conditionEvent.condition_type());
	if (classifyMissionsIt == missionComp->GetEventMissionsClassify().end()) {
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
		auto&mission = missionIter->second;

		// Update mission progress
		if (!UpdateMissionProgress(conditionEvent, mission)) {
			continue;
		}

		// Check if all conditions are now fulfilled
		if (!AreAllConditionsFulfilled(mission, missionId, missionComp)) {
			continue;
		}
		mission.set_status(MissionPbComp::E_MISSION_COMPLETE);
		completedMissionsThisTime.emplace(missionId);
		missionComp->GetMissionsComp().mutable_missions()->erase(missionIter);
	}

	// Process completion events for completed missions
	OnMissionCompletion(playerEntity, completedMissionsThisTime);
}

// Remove mission classification
void MissionSystem::RemoveMissionClassification(MissionsComp* missionComp, uint32_t missionId) {
	// Retrieve conditions from mission configuration
	const auto& configConditions = missionComp->GetMissionConfig()->GetConditionIds(missionId);

	// Remove mission classification based on condition type
	for (int32_t i = 0; i < configConditions.size(); ++i) {
		const auto* conditionRow = condition_config::GetSingleton().get(configConditions.Get(i));
		if (nullptr == conditionRow) {
			continue;
		}
		missionComp->GetEventMissionsClassify()[conditionRow->condition_type()].erase(missionId);
	}
}

// Delete mission classification
void MissionSystem::DeleteMissionClassification(entt::entity playerEntity, uint32_t missionId) {
	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComp>(playerEntity);
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
bool MissionSystem::UpdateMissionProgress(const MissionConditionEvent& conditionEvent, MissionPbComp& mission) {
	// Ignore if no conditions are provided
	if (conditionEvent.condtion_ids().empty()) {
		return false;
	}

	// Retrieve mission component for the player
	const auto* const missionComp = tls.registry.try_get<MissionsComp>(entt::to_entity(conditionEvent.entity()));
	if (nullptr == missionComp) {
		return false;
	}

	bool missionUpdated = false;

	// Retrieve mission conditions from configuration
	const auto& missionConditions = missionComp->GetMissionConfig()->GetConditionIds(mission.id());

	// Iterate through mission conditions and update progress
	for (int32_t i = 0; i < mission.progress_size() && i < missionConditions.size(); ++i) {
		const auto* const conditionRow = condition_config::GetSingleton().get(missionConditions.at(i));
		if (nullptr == conditionRow) {
			continue;
		}

		// Update progress if condition matches event
		if (UpdateProgressIfConditionMatches(conditionEvent, mission, i, conditionRow)) {
			missionUpdated = true;
		}
	}

	// Update mission status based on progress
	if (missionUpdated) {
		UpdateMissionStatus(mission, missionConditions);
	}

	return missionUpdated;
}

// Update mission progress if condition matches event
bool MissionSystem::UpdateProgressIfConditionMatches(const MissionConditionEvent& conditionEvent, MissionPbComp& mission, int index, const condition_row* conditionRow) {
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
		if (configConditions.size() > index) {
			++configConditionCount;
			if (std::find(conditionEvent.condtion_ids().begin(), conditionEvent.condtion_ids().end(), configConditions.Get(index)) != conditionEvent.condtion_ids().end()) {
				++matchConditionCount;
			}
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
void MissionSystem::UpdateMissionStatus(MissionPbComp& mission, const google::protobuf::RepeatedField<uint32_t>& missionConditions) {
	// Iterate through mission conditions and update progress
	for (int32_t i = 0; i < mission.progress_size() && i < missionConditions.size(); ++i) {
		const auto* const conditionRow = condition_config::GetSingleton().get(missionConditions.at(i));
		if (nullptr == conditionRow) {
			continue;
		}

		auto newProgress = mission.progress(i);

		// Clamp progress to condition amount if exceeded
		if (!IsConditionFulfilled(conditionRow->id(), newProgress)) {
			continue;
		}

		mission.set_progress(i, std::min(newProgress, conditionRow->amount()));
	}
}

// Process completion events for completed missions
void MissionSystem::OnMissionCompletion(entt::entity playerEntity, const std::unordered_set<uint32_t>& completedMissionsThisTime) {
	// Ignore if no missions are completed
	if (completedMissionsThisTime.empty()) {
		return;
	}

	// Retrieve mission component for the player
	auto* const missionComp = tls.registry.try_get<MissionsComp>(playerEntity);
	if (nullptr == missionComp) {
		return;
	}

	// Process each completed mission
	for (const auto& missionId : completedMissionsThisTime) {
		// Delete mission classification
		DeleteMissionClassification(playerEntity, missionId);

		// Retrieve mission reward component for the player
		auto* const missionReward = tls.registry.try_get<MissionRewardPbComp>(playerEntity);

		// Create mission condition event
		MissionConditionEvent missionConditionEvent;
		missionConditionEvent.set_entity(entt::to_integral(playerEntity));
		missionConditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionCompleteMission));
		missionConditionEvent.set_amount(1);

		// Process mission completion rewards and events
		for (const auto& missionId : completedMissionsThisTime) {
			// Mark mission as complete
			missionComp->GetMissionsComp().mutable_complete_missions()->insert({ missionId, true });

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
			const auto& nextMissions = missionComp->GetMissionConfig()->getNextMissionIds(missionId);

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
