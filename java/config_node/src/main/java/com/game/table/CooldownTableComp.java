
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for CooldownTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class CooldownTableComp {

    private CooldownTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(CooldownTable row) {
            return new Id(row.getId());
        }
    }

    public record Duration(int value) {
        public static Duration from(CooldownTable row) {
            return new Duration(row.getDuration());
        }
    }

}