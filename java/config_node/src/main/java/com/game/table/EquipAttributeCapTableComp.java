
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for EquipAttributeCapTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class EquipAttributeCapTableComp {

    private EquipAttributeCapTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(EquipAttributeCapTable row) {
            return new Id(row.getId());
        }
    }

    public record Attr_id(int value) {
        public static Attr_id from(EquipAttributeCapTable row) {
            return new Attr_id(row.getAttrId());
        }
    }

    public record Level(int value) {
        public static Level from(EquipAttributeCapTable row) {
            return new Level(row.getLevel());
        }
    }

    public record Min_value(int value) {
        public static Min_value from(EquipAttributeCapTable row) {
            return new Min_value(row.getMinValue());
        }
    }

    public record Cap(int value) {
        public static Cap from(EquipAttributeCapTable row) {
            return new Cap(row.getCap());
        }
    }

}