package com.game.table;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreign key helpers for MysteryMerchantGoodsTable.
 * DO NOT EDIT -- regenerate from Excel via Data Table Exporter.
 */
public final class MysteryMerchantGoodsTableForeignKeys {
    private MysteryMerchantGoodsTableForeignKeys() {}

    /** Resolve MysteryMerchantGoods.item_id -> Item row. */
    public static ItemTable getItemIdRow(MysteryMerchantGoodsTable row) {
        return ItemTableManager.getInstance().findById(row.getItemId());
    }

    /** Resolve MysteryMerchantGoods.item_id -> Item row (by MysteryMerchantGoods id). */
    public static ItemTable getItemIdRow(int tableId) {
        MysteryMerchantGoodsTable row = MysteryMerchantGoodsTableManager.getInstance().findById(tableId);
        if (row == null) { return null; }
        return getItemIdRow(row);
    }

    // ---- Reverse FK (HasMany): find source rows by FK column value ----

    /** Reverse FK: find all MysteryMerchantGoods rows whose item_id == key. */
    public static List<MysteryMerchantGoodsTable> findRowsByItemId(int key) {
        return MysteryMerchantGoodsTableManager.getInstance().getByItemId(key);
    }

}
