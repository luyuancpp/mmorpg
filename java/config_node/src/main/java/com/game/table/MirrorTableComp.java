
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for MirrorTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class MirrorTableComp {

    private MirrorTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(MirrorTable row) {
            return new Id(row.getId());
        }
    }

    public record Scene_id(int value) {
        public static Scene_id from(MirrorTable row) {
            return new Scene_id(row.getSceneId());
        }
    }

    public record Main_scene_id(int value) {
        public static Main_scene_id from(MirrorTable row) {
            return new Main_scene_id(row.getMainSceneId());
        }
    }

}