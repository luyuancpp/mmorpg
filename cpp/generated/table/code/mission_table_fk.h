#pragma once
#include <vector>
#include "mission_table.h"

#include "condition_table.h"

#include "reward_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for MissionTable
// ---------------------------------------------------------------------------

/// Resolve Mission.reward_id -> Reward row.
inline const RewardTable* GetMissionRewardIdRow(const MissionTable& row) {
    auto [ptr, _] = RewardTableManager::Instance().FindByIdSilent(row.reward_id());
    return ptr;
}

/// Resolve Mission.reward_id -> Reward row (by Mission id).
inline const RewardTable* GetMissionRewardIdRow(uint32_t tableId) {
    auto [row, _] = MissionTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return nullptr;
    return GetMissionRewardIdRow(*row);
}

/// Resolve Mission.condition_id[] -> Condition rows.
inline std::vector<const ConditionTable*> GetMissionConditionIdRows(const MissionTable& row) {
    std::vector<const ConditionTable*> result;
    for (auto id : row.condition_id()) {
        auto [ptr, _] = ConditionTableManager::Instance().FindByIdSilent(id);
        if (ptr) result.push_back(ptr);
    }
    return result;
}

/// Resolve Mission.condition_id[] -> Condition rows (by Mission id).
inline std::vector<const ConditionTable*> GetMissionConditionIdRows(uint32_t tableId) {
    auto [row, _] = MissionTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return {};
    return GetMissionConditionIdRows(*row);
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

/// Reverse FK: find all Mission rows whose reward_id == key.
inline const std::vector<const MissionTable*>& FindMissionRowsByRewardId(uint32_t key) {
    return MissionTableManager::Instance().GetByRewardId(key);
}
