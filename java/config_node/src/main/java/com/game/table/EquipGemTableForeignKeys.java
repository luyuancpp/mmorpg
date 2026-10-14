package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for EquipGemTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class EquipGemTableForeignKeys {
    private EquipGemTableForeignKeys() {}

    /** Resolve EquipGem.id -> Item row. */
    public static ItemTable getIdRow(EquipGemTable row) {
        return ItemTableManager.getInstance().findById(row.getId());
    }

    /** Resolve EquipGem.id -> Item row (by EquipGem id). */
    public static ItemTable getIdRow(int tableId) {
        EquipGemTable row = EquipGemTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getIdRow(row);
    }

    /** Resolve EquipGem.attr_id -> EquipAttribute row. */
    public static EquipAttributeTable getAttrIdRow(EquipGemTable row) {
        return EquipAttributeTableManager.getInstance().findById(row.getAttrId());
    }

    /** Resolve EquipGem.attr_id -> EquipAttribute row (by EquipGem id). */
    public static EquipAttributeTable getAttrIdRow(int tableId) {
        EquipGemTable row = EquipGemTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getAttrIdRow(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all EquipGem rows whose id == key. */
    public static List<EquipGemTable> findRowsById(int key) {
        return EquipGemTableManager.getInstance().getById(key);
    }

    /** Reverse FK: find all EquipGem rows whose attr_id == key. */
    public static List<EquipGemTable> findRowsByAttrId(int key) {
        return EquipGemTableManager.getInstance().getByAttrId(key);
    }

}
