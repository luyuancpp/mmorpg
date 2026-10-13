
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for EquipAffixRuleTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class EquipAffixRuleTableComp {

    private EquipAffixRuleTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(EquipAffixRuleTable row) {
            return new Id(row.getId());
        }
    }

    public record Blue_min(int value) {
        public static Blue_min from(EquipAffixRuleTable row) {
            return new Blue_min(row.getBlueMin());
        }
    }

    public record Blue_max(int value) {
        public static Blue_max from(EquipAffixRuleTable row) {
            return new Blue_max(row.getBlueMax());
        }
    }

    public record Pink_rate(int value) {
        public static Pink_rate from(EquipAffixRuleTable row) {
            return new Pink_rate(row.getPinkRate());
        }
    }

    public record Yellow_rate(int value) {
        public static Yellow_rate from(EquipAffixRuleTable row) {
            return new Yellow_rate(row.getYellowRate());
        }
    }

}