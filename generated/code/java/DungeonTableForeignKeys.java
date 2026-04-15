package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for DungeonTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class DungeonTableForeignKeys {
    private DungeonTableForeignKeys() {}

    /** Resolve Dungeon.scene_id -> BaseScene row. */
    public static BaseSceneTable getSceneIdRow(DungeonTable row) {
        return BaseSceneTableManager.getInstance().findById(row.getSceneId());
    }

    /** Resolve Dungeon.scene_id -> BaseScene row (by Dungeon id). */
    public static BaseSceneTable getSceneIdRow(int tableId) {
        DungeonTable row = DungeonTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getSceneIdRow(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all Dungeon rows whose scene_id == key. */
    public static List<DungeonTable> findRowsBySceneId(int key) {
        return DungeonTableManager.getInstance().getBySceneId(key);
    }

}
