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

}
