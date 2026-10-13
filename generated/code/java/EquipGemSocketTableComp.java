
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for EquipGemSocketTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class EquipGemSocketTableComp {

    private EquipGemSocketTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(EquipGemSocketTable row) {
            return new Id(row.getId());
        }
    }

    public record Equip_kind(int value) {
        public static Equip_kind from(EquipGemSocketTable row) {
            return new Equip_kind(row.getEquipKind());
        }
    }

    public record Socket(int value) {
        public static Socket from(EquipGemSocketTable row) {
            return new Socket(row.getSocket());
        }
    }

    public record Gem_type(int value) {
        public static Gem_type from(EquipGemSocketTable row) {
            return new Gem_type(row.getGemType());
        }
    }

    public record Name(String value) {
        public static Name from(EquipGemSocketTable row) {
            return new Name(row.getName());
        }
    }

}