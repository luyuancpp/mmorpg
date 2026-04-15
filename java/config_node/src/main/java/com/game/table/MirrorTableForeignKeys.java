package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for MirrorTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class MirrorTableForeignKeys {
    private MirrorTableForeignKeys() {}

    /** Resolve Mirror.scene_id -> BaseScene row. */
    public static BaseSceneTable getSceneIdRow(MirrorTable row) {
        return BaseSceneTableManager.getInstance().findById(row.getSceneId());
    }

    /** Resolve Mirror.main_scene_id -> World row. */
    public static WorldTable getMainSceneIdRow(MirrorTable row) {
        return WorldTableManager.getInstance().findById(row.getMainSceneId());
    }

}
