package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for MissionTable
// ---------------------------------------------------------------------------

// GetMissionRewardIdRow resolves Mission.reward_id -> Reward row.
func GetMissionRewardIdRow(row *pb.MissionTable) (*pb.RewardTable, bool) {
    return RewardTableManagerInstance.FindById(row.RewardId)
}

// GetMissionRewardIdRowById resolves Mission.reward_id -> Reward row (by Mission id).
func GetMissionRewardIdRowById(tableId uint32) (*pb.RewardTable, bool) {
    row, ok := MissionTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetMissionRewardIdRow(row)
}

// GetMissionConditionIdRows resolves Mission.condition_id[] -> Condition rows.
func GetMissionConditionIdRows(row *pb.MissionTable) []*pb.ConditionTable {
    var result []*pb.ConditionTable
    for _, id := range row.ConditionId {
        if r, ok := ConditionTableManagerInstance.FindById(id); ok {
            result = append(result, r)
        }
    }
    return result
}

// GetMissionConditionIdRowsById resolves Mission.condition_id[] -> Condition rows (by Mission id).
func GetMissionConditionIdRowsById(tableId uint32) []*pb.ConditionTable {
    row, ok := MissionTableManagerInstance.FindById(tableId)
    if !ok {
        return nil
    }
    return GetMissionConditionIdRows(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindMissionRowsByRewardId returns all Mission rows whose reward_id == key.
func FindMissionRowsByRewardId(key uint32) []*pb.MissionTable {
    return MissionTableManagerInstance.GetByRewardId(key)
}
