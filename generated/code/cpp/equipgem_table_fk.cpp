#include "equipgem_table_fk.h"
#include "equipgem_table.h"
#include "equipattribute_table.h"
#include "item_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for EquipGemTable
// ---------------------------------------------------------------------------

/// Resolve EquipGem.id -> Item row.
const ItemTable* GetEquipGemIdRow(const EquipGemTable& row) {
    auto [ptr, _] = ItemTableManager::Instance().FindByIdSilent(row.id());
    return ptr;
}

/// Resolve EquipGem.id -> Item row (by EquipGem id).
const ItemTable* GetEquipGemIdRow(uint32_t tableId) {
    auto [row, _] = EquipGemTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return nullptr;
    return GetEquipGemIdRow(*row);
}

/// Resolve EquipGem.attr_id -> EquipAttribute row.
const EquipAttributeTable* GetEquipGemAttrIdRow(const EquipGemTable& row) {
    auto [ptr, _] = EquipAttributeTableManager::Instance().FindByIdSilent(row.attr_id());
    return ptr;
}

/// Resolve EquipGem.attr_id -> EquipAttribute row (by EquipGem id).
const EquipAttributeTable* GetEquipGemAttrIdRow(uint32_t tableId) {
    auto [row, _] = EquipGemTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return nullptr;
    return GetEquipGemAttrIdRow(*row);
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// 只碰本表的 manager,不需要任何目标表的类型。
// ---------------------------------------------------------------------------

/// Reverse FK: find all EquipGem rows whose id == key.
const std::vector<const EquipGemTable*>& FindEquipGemRowsById(uint32_t key) {
    return EquipGemTableManager::Instance().GetById(key);
}

/// Reverse FK: find all EquipGem rows whose attr_id == key.
const std::vector<const EquipGemTable*>& FindEquipGemRowsByAttrId(uint32_t key) {
    return EquipGemTableManager::Instance().GetByAttrId(key);
}
