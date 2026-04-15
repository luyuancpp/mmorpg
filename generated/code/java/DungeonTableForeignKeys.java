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

}
