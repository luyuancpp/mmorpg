package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for EquipAffixPoolTable
// ---------------------------------------------------------------------------

// GetEquipAffixPoolAttrIdRow resolves EquipAffixPool.attr_id -> EquipAttribute row.
func GetEquipAffixPoolAttrIdRow(row *pb.EquipAffixPoolTable) (*pb.EquipAttributeTable, bool) {
    return EquipAttributeTableManagerInstance.FindById(row.AttrId)
}

// GetEquipAffixPoolAttrIdRowById resolves EquipAffixPool.attr_id -> EquipAttribute row (by EquipAffixPool id).
func GetEquipAffixPoolAttrIdRowById(tableId uint32) (*pb.EquipAttributeTable, bool) {
    row, ok := EquipAffixPoolTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetEquipAffixPoolAttrIdRow(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindEquipAffixPoolRowsByAttrId returns all EquipAffixPool rows whose attr_id == key.
func FindEquipAffixPoolRowsByAttrId(key uint32) []*pb.EquipAffixPoolTable {
    return EquipAffixPoolTableManagerInstance.GetByAttrId(key)
}
