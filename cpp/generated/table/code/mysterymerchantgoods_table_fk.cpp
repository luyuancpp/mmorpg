#include "mysterymerchantgoods_table_fk.h"
#include "mysterymerchantgoods_table.h"
#include "item_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for MysteryMerchantGoodsTable
// ---------------------------------------------------------------------------

/// Resolve MysteryMerchantGoods.item_id -> Item row.
const ItemTable* GetMysteryMerchantGoodsItemIdRow(const MysteryMerchantGoodsTable& row) {
    auto [ptr, _] = ItemTableManager::Instance().FindByIdSilent(row.item_id());
    return ptr;
}

/// Resolve MysteryMerchantGoods.item_id -> Item row (by MysteryMerchantGoods id).
const ItemTable* GetMysteryMerchantGoodsItemIdRow(uint32_t tableId) {
    auto [row, _] = MysteryMerchantGoodsTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return nullptr;
    return GetMysteryMerchantGoodsItemIdRow(*row);
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// 只碰本表的 manager,不需要任何目标表的类型。
// ---------------------------------------------------------------------------

/// Reverse FK: find all MysteryMerchantGoods rows whose item_id == key.
const std::vector<const MysteryMerchantGoodsTable*>& FindMysteryMerchantGoodsRowsByItemId(uint32_t key) {
    return MysteryMerchantGoodsTableManager::Instance().GetByItemId(key);
}
