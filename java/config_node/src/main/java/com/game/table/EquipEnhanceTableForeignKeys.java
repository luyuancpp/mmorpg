package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for EquipEnhanceTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class EquipEnhanceTableForeignKeys {
    private EquipEnhanceTableForeignKeys() {}

    /** Resolve EquipEnhance.material_id -> Item row. */
    public static ItemTable getMaterialIdRow(EquipEnhanceTable row) {
        return ItemTableManager.getInstance().findById(row.getMaterialId());
    }

    /** Resolve EquipEnhance.material_id -> Item row (by EquipEnhance id). */
    public static ItemTable getMaterialIdRow(int tableId) {
        EquipEnhanceTable row = EquipEnhanceTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getMaterialIdRow(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all EquipEnhance rows whose material_id == key. */
    public static List<EquipEnhanceTable> findRowsByMaterialId(int key) {
        return EquipEnhanceTableManager.getInstance().getByMaterialId(key);
    }

}
