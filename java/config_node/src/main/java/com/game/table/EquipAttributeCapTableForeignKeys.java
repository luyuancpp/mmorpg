package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for EquipAttributeCapTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class EquipAttributeCapTableForeignKeys {
    private EquipAttributeCapTableForeignKeys() {}

    /** Resolve EquipAttributeCap.attr_id -> EquipAttribute row. */
    public static EquipAttributeTable getAttrIdRow(EquipAttributeCapTable row) {
        return EquipAttributeTableManager.getInstance().findById(row.getAttrId());
    }

    /** Resolve EquipAttributeCap.attr_id -> EquipAttribute row (by EquipAttributeCap id). */
    public static EquipAttributeTable getAttrIdRow(int tableId) {
        EquipAttributeCapTable row = EquipAttributeCapTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getAttrIdRow(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all EquipAttributeCap rows whose attr_id == key. */
    public static List<EquipAttributeCapTable> findRowsByAttrId(int key) {
        return EquipAttributeCapTableManager.getInstance().getByAttrId(key);
    }

}
