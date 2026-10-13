package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for ItemTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class ItemTableForeignKeys {
    private ItemTableForeignKeys() {}

    /** Resolve Item.affix_rule -> EquipAffixRule row. */
    public static EquipAffixRuleTable getAffixRuleRow(ItemTable row) {
        return EquipAffixRuleTableManager.getInstance().findById(row.getAffixRule());
    }

    /** Resolve Item.affix_rule -> EquipAffixRule row (by Item id). */
    public static EquipAffixRuleTable getAffixRuleRow(int tableId) {
        ItemTable row = ItemTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getAffixRuleRow(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all Item rows whose affix_rule == key. */
    public static List<ItemTable> findRowsByAffixRule(int key) {
        return ItemTableManager.getInstance().getByAffixRule(key);
    }

}
