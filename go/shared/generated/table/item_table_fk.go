package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for ItemTable
// ---------------------------------------------------------------------------

// GetItemAffixRuleRow resolves Item.affix_rule -> EquipAffixRule row.
func GetItemAffixRuleRow(row *pb.ItemTable) (*pb.EquipAffixRuleTable, bool) {
    return EquipAffixRuleTableManagerInstance.FindById(row.AffixRule)
}

// GetItemAffixRuleRowById resolves Item.affix_rule -> EquipAffixRule row (by Item id).
func GetItemAffixRuleRowById(tableId uint32) (*pb.EquipAffixRuleTable, bool) {
    row, ok := ItemTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetItemAffixRuleRow(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindItemRowsByAffixRule returns all Item rows whose affix_rule == key.
func FindItemRowsByAffixRule(key uint32) []*pb.ItemTable {
    return ItemTableManagerInstance.GetByAffixRule(key)
}
