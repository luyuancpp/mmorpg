
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for EquipAttributeTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class EquipAttributeTableComp {

    private EquipAttributeTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(EquipAttributeTable row) {
            return new Id(row.getId());
        }
    }

    public record Name(String value) {
        public static Name from(EquipAttributeTable row) {
            return new Name(row.getName());
        }
    }

    public record Effect(int value) {
        public static Effect from(EquipAttributeTable row) {
            return new Effect(row.getEffect());
        }
    }

    public record Effect_param(int value) {
        public static Effect_param from(EquipAttributeTable row) {
            return new Effect_param(row.getEffectParam());
        }
    }

    public record Percent(int value) {
        public static Percent from(EquipAttributeTable row) {
            return new Percent(row.getPercent());
        }
    }

    public record Sort(int value) {
        public static Sort from(EquipAttributeTable row) {
            return new Sort(row.getSort());
        }
    }

}