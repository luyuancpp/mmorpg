
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for RewardTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class RewardTableComp {

    private RewardTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(RewardTable row) {
            return new Id(row.getId());
        }
    }

}