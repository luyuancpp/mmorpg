#include "equipenhance_table_fk.h"
#include "equipenhance_table.h"
#include "item_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for EquipEnhanceTable
// ---------------------------------------------------------------------------

/// Resolve EquipEnhance.material_id -> Item row.
const ItemTable* GetEquipEnhanceMaterialIdRow(const EquipEnhanceTable& row) {
    auto [ptr, _] = ItemTableManager::Instance().FindByIdSilent(row.material_id());
    return ptr;
}

/// Resolve EquipEnhance.material_id -> Item row (by EquipEnhance id).
const ItemTable* GetEquipEnhanceMaterialIdRow(uint32_t tableId) {
    auto [row, _] = EquipEnhanceTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return nullptr;
    return GetEquipEnhanceMaterialIdRow(*row);
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// 只碰本表的 manager,不需要任何目标表的类型。
// ---------------------------------------------------------------------------

/// Reverse FK: find all EquipEnhance rows whose material_id == key.
const std::vector<const EquipEnhanceTable*>& FindEquipEnhanceRowsByMaterialId(uint32_t key) {
    return EquipEnhanceTableManager::Instance().GetByMaterialId(key);
}
