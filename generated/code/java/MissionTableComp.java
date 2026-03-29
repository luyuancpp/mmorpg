
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for MissionTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class MissionTableComp {

    private MissionTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(MissionTable row) {
            return new Id(row.getId());
        }
    }

    public record Mission_type(int value) {
        public static Mission_type from(MissionTable row) {
            return new Mission_type(row.getMissionType());
        }
    }

    public record Mission_sub_type(int value) {
        public static Mission_sub_type from(MissionTable row) {
            return new Mission_sub_type(row.getMissionSubType());
        }
    }

    public record Condition_order(int value) {
        public static Condition_order from(MissionTable row) {
            return new Condition_order(row.getConditionOrder());
        }
    }

    public record Auto_reward(int value) {
        public static Auto_reward from(MissionTable row) {
            return new Auto_reward(row.getAutoReward());
        }
    }

    public record Reward_id(int value) {
        public static Reward_id from(MissionTable row) {
            return new Reward_id(row.getRewardId());
        }
    }

    public record Condition_id(List<Integer> values) {
        public static Condition_id from(MissionTable row) {
            return new Condition_id(row.getConditionIdList());
        }
    }

    public record Next_mission_id(List<Integer> values) {
        public static Next_mission_id from(MissionTable row) {
            return new Next_mission_id(row.getNextMissionIdList());
        }
    }

    public record Target_count(List<Integer> values) {
        public static Target_count from(MissionTable row) {
            return new Target_count(row.getTargetCountList());
        }
    }

}