
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for MonsterBaseTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class MonsterBaseTableComp {

    private MonsterBaseTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(MonsterBaseTable row) {
            return new Id(row.getId());
        }
    }

}