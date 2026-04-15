package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for MissionTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class MissionTableForeignKeys {
    private MissionTableForeignKeys() {}

    /** Resolve Mission.reward_id -> Reward row. */
    public static RewardTable getRewardIdRow(MissionTable row) {
        return RewardTableManager.getInstance().findById(row.getRewardId());
    }

    /** Resolve Mission.reward_id -> Reward row (by Mission id). */
    public static RewardTable getRewardIdRow(int tableId) {
        MissionTable row = MissionTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getRewardIdRow(row);
    }

    /** Resolve Mission.condition_id[] -> Condition rows. */
    public static List<ConditionTable> getConditionIdRows(MissionTable row) {
        List<ConditionTable> result = new ArrayList<>();
        for (int id : row.getConditionIdList()) {
            ConditionTable r = ConditionTableManager.getInstance().findById(id);
            if (r != null) { result.add(r); }
        }
        return result;
    }

    /** Resolve Mission.condition_id[] -> Condition rows (by Mission id). */
    public static List<ConditionTable> getConditionIdRows(int tableId) {
        MissionTable row = MissionTableManager.getInstance().findById(tableId);
        if (row == null) { return List.of(); }
        return getConditionIdRows(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all Mission rows whose reward_id == key. */
    public static List<MissionTable> findRowsByRewardId(int key) {
        return MissionTableManager.getInstance().getByRewardId(key);
    }

}
