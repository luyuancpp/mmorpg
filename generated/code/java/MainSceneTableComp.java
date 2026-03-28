
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for MainSceneTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class MainSceneTableComp {

    private MainSceneTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(MainSceneTable row) {
            return new Id(row.getId());
        }
    }

    public record Nav_bin_file(String value) {
        public static Nav_bin_file from(MainSceneTable row) {
            return new Nav_bin_file(row.getNavBinFile());
        }
    }

}