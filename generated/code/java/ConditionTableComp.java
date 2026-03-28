
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for ConditionTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class ConditionTableComp {

    private ConditionTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(ConditionTable row) {
            return new Id(row.getId());
        }
    }

    public record Condition_category(int value) {
        public static Condition_category from(ConditionTable row) {
            return new Condition_category(row.getConditionCategory());
        }
    }

    public record Valid_duration(long value) {
        public static Valid_duration from(ConditionTable row) {
            return new Valid_duration(row.getValidDuration());
        }
    }

    public record Quantity_type(int value) {
        public static Quantity_type from(ConditionTable row) {
            return new Quantity_type(row.getQuantityType());
        }
    }

    public record Target_count(int value) {
        public static Target_count from(ConditionTable row) {
            return new Target_count(row.getTargetCount());
        }
    }

    public record Comparison_op(int value) {
        public static Comparison_op from(ConditionTable row) {
            return new Comparison_op(row.getComparisonOp());
        }
    }

    public record Condition1(List<Integer> values) {
        public static Condition1 from(ConditionTable row) {
            return new Condition1(row.getCondition1List());
        }
    }

    public record Condition2(List<Integer> values) {
        public static Condition2 from(ConditionTable row) {
            return new Condition2(row.getCondition2List());
        }
    }

    public record Condition3(List<Integer> values) {
        public static Condition3 from(ConditionTable row) {
            return new Condition3(row.getCondition3List());
        }
    }

    public record Condition4(List<Integer> values) {
        public static Condition4 from(ConditionTable row) {
            return new Condition4(row.getCondition4List());
        }
    }

}