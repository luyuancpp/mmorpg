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

    /** Resolve Mirror.scene_id -> BaseScene row (by Mirror id). */
    public static BaseSceneTable getSceneIdRow(int tableId) {
        MirrorTable row = MirrorTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getSceneIdRow(row);
    }

    /** Resolve Mirror.main_scene_id -> World row. */
    public static WorldTable getMainSceneIdRow(MirrorTable row) {
        return WorldTableManager.getInstance().findById(row.getMainSceneId());
    }

    /** Resolve Mirror.main_scene_id -> World row (by Mirror id). */
    public static WorldTable getMainSceneIdRow(int tableId) {
        MirrorTable row = MirrorTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getMainSceneIdRow(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all Mirror rows whose scene_id == key. */
    public static List<MirrorTable> findRowsBySceneId(int key) {
        return MirrorTableManager.getInstance().getBySceneId(key);
    }

    /** Reverse FK: find all Mirror rows whose main_scene_id == key. */
    public static List<MirrorTable> findRowsByMainSceneId(int key) {
        return MirrorTableManager.getInstance().getByMainSceneId(key);
    }

}
