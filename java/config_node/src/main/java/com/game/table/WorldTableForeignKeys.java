package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for WorldTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class WorldTableForeignKeys {
    private WorldTableForeignKeys() {}

    /** Resolve World.scene_id -> BaseScene row. */
    public static BaseSceneTable getSceneIdRow(WorldTable row) {
        return BaseSceneTableManager.getInstance().findById(row.getSceneId());
    }

    /** Resolve World.scene_id -> BaseScene row (by World id). */
    public static BaseSceneTable getSceneIdRow(int tableId) {
        WorldTable row = WorldTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getSceneIdRow(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all World rows whose scene_id == key. */
    public static List<WorldTable> findRowsBySceneId(int key) {
        return WorldTableManager.getInstance().getBySceneId(key);
    }

}
