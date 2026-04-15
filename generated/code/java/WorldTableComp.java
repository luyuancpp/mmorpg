
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for WorldTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class WorldTableComp {

    private WorldTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(WorldTable row) {
            return new Id(row.getId());
        }
    }

    public record Scene_id(int value) {
        public static Scene_id from(WorldTable row) {
            return new Scene_id(row.getSceneId());
        }
    }

}