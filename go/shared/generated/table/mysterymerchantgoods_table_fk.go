package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for MysteryMerchantGoodsTable
// ---------------------------------------------------------------------------

// GetMysteryMerchantGoodsItemIdRow resolves MysteryMerchantGoods.item_id -> Item row.
func GetMysteryMerchantGoodsItemIdRow(row *pb.MysteryMerchantGoodsTable) (*pb.ItemTable, bool) {
    return ItemTableManagerInstance.FindById(row.ItemId)
}

// GetMysteryMerchantGoodsItemIdRowById resolves MysteryMerchantGoods.item_id -> Item row (by MysteryMerchantGoods id).
func GetMysteryMerchantGoodsItemIdRowById(tableId uint32) (*pb.ItemTable, bool) {
    row, ok := MysteryMerchantGoodsTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetMysteryMerchantGoodsItemIdRow(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindMysteryMerchantGoodsRowsByItemId returns all MysteryMerchantGoods rows whose item_id == key.
func FindMysteryMerchantGoodsRowsByItemId(key uint32) []*pb.MysteryMerchantGoodsTable {
    return MysteryMerchantGoodsTableManagerInstance.GetByItemId(key)
}
