package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for EquipAttributeCapTable
// ---------------------------------------------------------------------------

// GetEquipAttributeCapAttrIdRow resolves EquipAttributeCap.attr_id -> EquipAttribute row.
func GetEquipAttributeCapAttrIdRow(row *pb.EquipAttributeCapTable) (*pb.EquipAttributeTable, bool) {
    return EquipAttributeTableManagerInstance.FindById(row.AttrId)
}

// GetEquipAttributeCapAttrIdRowById resolves EquipAttributeCap.attr_id -> EquipAttribute row (by EquipAttributeCap id).
func GetEquipAttributeCapAttrIdRowById(tableId uint32) (*pb.EquipAttributeTable, bool) {
    row, ok := EquipAttributeCapTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetEquipAttributeCapAttrIdRow(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindEquipAttributeCapRowsByAttrId returns all EquipAttributeCap rows whose attr_id == key.
func FindEquipAttributeCapRowsByAttrId(key uint32) []*pb.EquipAttributeCapTable {
    return EquipAttributeCapTableManagerInstance.GetByAttrId(key)
}
