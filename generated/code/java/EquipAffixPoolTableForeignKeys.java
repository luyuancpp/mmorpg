package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for EquipAffixPoolTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class EquipAffixPoolTableForeignKeys {
    private EquipAffixPoolTableForeignKeys() {}

    /** Resolve EquipAffixPool.attr_id -> EquipAttribute row. */
    public static EquipAttributeTable getAttrIdRow(EquipAffixPoolTable row) {
        return EquipAttributeTableManager.getInstance().findById(row.getAttrId());
    }

    /** Resolve EquipAffixPool.attr_id -> EquipAttribute row (by EquipAffixPool id). */
    public static EquipAttributeTable getAttrIdRow(int tableId) {
        EquipAffixPoolTable row = EquipAffixPoolTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getAttrIdRow(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all EquipAffixPool rows whose attr_id == key. */
    public static List<EquipAffixPoolTable> findRowsByAttrId(int key) {
        return EquipAffixPoolTableManager.getInstance().getByAttrId(key);
    }

}
