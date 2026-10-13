#include "equipaffixpool_table_fk.h"
#include "equipaffixpool_table.h"
#include "equipattribute_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for EquipAffixPoolTable
// ---------------------------------------------------------------------------

/// Resolve EquipAffixPool.attr_id -> EquipAttribute row.
const EquipAttributeTable* GetEquipAffixPoolAttrIdRow(const EquipAffixPoolTable& row) {
    auto [ptr, _] = EquipAttributeTableManager::Instance().FindByIdSilent(row.attr_id());
    return ptr;
}

/// Resolve EquipAffixPool.attr_id -> EquipAttribute row (by EquipAffixPool id).
const EquipAttributeTable* GetEquipAffixPoolAttrIdRow(uint32_t tableId) {
    auto [row, _] = EquipAffixPoolTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return nullptr;
    return GetEquipAffixPoolAttrIdRow(*row);
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// 只碰本表的 manager,不需要任何目标表的类型。
// ---------------------------------------------------------------------------

/// Reverse FK: find all EquipAffixPool rows whose attr_id == key.
const std::vector<const EquipAffixPoolTable*>& FindEquipAffixPoolRowsByAttrId(uint32_t key) {
    return EquipAffixPoolTableManager::Instance().GetByAttrId(key);
}
