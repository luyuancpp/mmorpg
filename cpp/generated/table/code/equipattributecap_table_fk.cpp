#include "equipattributecap_table_fk.h"
#include "equipattributecap_table.h"
#include "equipattribute_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for EquipAttributeCapTable
// ---------------------------------------------------------------------------

/// Resolve EquipAttributeCap.attr_id -> EquipAttribute row.
const EquipAttributeTable* GetEquipAttributeCapAttrIdRow(const EquipAttributeCapTable& row) {
    auto [ptr, _] = EquipAttributeTableManager::Instance().FindByIdSilent(row.attr_id());
    return ptr;
}

/// Resolve EquipAttributeCap.attr_id -> EquipAttribute row (by EquipAttributeCap id).
const EquipAttributeTable* GetEquipAttributeCapAttrIdRow(uint32_t tableId) {
    auto [row, _] = EquipAttributeCapTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return nullptr;
    return GetEquipAttributeCapAttrIdRow(*row);
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// 只碰本表的 manager,不需要任何目标表的类型。
// ---------------------------------------------------------------------------

/// Reverse FK: find all EquipAttributeCap rows whose attr_id == key.
const std::vector<const EquipAttributeCapTable*>& FindEquipAttributeCapRowsByAttrId(uint32_t key) {
    return EquipAttributeCapTableManager::Instance().GetByAttrId(key);
}
