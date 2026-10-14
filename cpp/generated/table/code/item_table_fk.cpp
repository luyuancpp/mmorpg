#include "item_table_fk.h"
#include "item_table.h"
#include "equipaffixrule_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for ItemTable
// ---------------------------------------------------------------------------

/// Resolve Item.affix_rule -> EquipAffixRule row.
const EquipAffixRuleTable* GetItemAffixRuleRow(const ItemTable& row) {
    auto [ptr, _] = EquipAffixRuleTableManager::Instance().FindByIdSilent(row.affix_rule());
    return ptr;
}

/// Resolve Item.affix_rule -> EquipAffixRule row (by Item id).
const EquipAffixRuleTable* GetItemAffixRuleRow(uint32_t tableId) {
    auto [row, _] = ItemTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return nullptr;
    return GetItemAffixRuleRow(*row);
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// 只碰本表的 manager,不需要任何目标表的类型。
// ---------------------------------------------------------------------------

/// Reverse FK: find all Item rows whose affix_rule == key.
const std::vector<const ItemTable*>& FindItemRowsByAffixRule(uint32_t key) {
    return ItemTableManager::Instance().GetByAffixRule(key);
}
