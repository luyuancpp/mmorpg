
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for EquipGemTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class EquipGemTableComp {

    private EquipGemTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(EquipGemTable row) {
            return new Id(row.getId());
        }
    }

    public record Gem_type(int value) {
        public static Gem_type from(EquipGemTable row) {
            return new Gem_type(row.getGemType());
        }
    }

    public record Attr_id(int value) {
        public static Attr_id from(EquipGemTable row) {
            return new Attr_id(row.getAttrId());
        }
    }

    public record Value(int value) {
        public static Value from(EquipGemTable row) {
            return new Value(row.getValue());
        }
    }

}