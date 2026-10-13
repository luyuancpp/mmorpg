
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for EquipAffixPoolTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class EquipAffixPoolTableComp {

    private EquipAffixPoolTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(EquipAffixPoolTable row) {
            return new Id(row.getId());
        }
    }

    public record Pool_id(int value) {
        public static Pool_id from(EquipAffixPoolTable row) {
            return new Pool_id(row.getPoolId());
        }
    }

    public record Attr_id(int value) {
        public static Attr_id from(EquipAffixPoolTable row) {
            return new Attr_id(row.getAttrId());
        }
    }

    public record Weight(int value) {
        public static Weight from(EquipAffixPoolTable row) {
            return new Weight(row.getWeight());
        }
    }

}