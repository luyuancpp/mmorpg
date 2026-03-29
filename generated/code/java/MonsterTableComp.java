
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for MonsterTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class MonsterTableComp {

    private MonsterTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(MonsterTable row) {
            return new Id(row.getId());
        }
    }

}