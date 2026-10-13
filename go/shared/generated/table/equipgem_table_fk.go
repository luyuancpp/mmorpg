package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for EquipGemTable
// ---------------------------------------------------------------------------

// GetEquipGemIdRow resolves EquipGem.id -> Item row.
func GetEquipGemIdRow(row *pb.EquipGemTable) (*pb.ItemTable, bool) {
    return ItemTableManagerInstance.FindById(row.Id)
}

// GetEquipGemIdRowById resolves EquipGem.id -> Item row (by EquipGem id).
func GetEquipGemIdRowById(tableId uint32) (*pb.ItemTable, bool) {
    row, ok := EquipGemTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetEquipGemIdRow(row)
}

// GetEquipGemAttrIdRow resolves EquipGem.attr_id -> EquipAttribute row.
func GetEquipGemAttrIdRow(row *pb.EquipGemTable) (*pb.EquipAttributeTable, bool) {
    return EquipAttributeTableManagerInstance.FindById(row.AttrId)
}

// GetEquipGemAttrIdRowById resolves EquipGem.attr_id -> EquipAttribute row (by EquipGem id).
func GetEquipGemAttrIdRowById(tableId uint32) (*pb.EquipAttributeTable, bool) {
    row, ok := EquipGemTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetEquipGemAttrIdRow(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindEquipGemRowsById returns all EquipGem rows whose id == key.
func FindEquipGemRowsById(key uint32) []*pb.EquipGemTable {
    return EquipGemTableManagerInstance.GetById(key)
}

// FindEquipGemRowsByAttrId returns all EquipGem rows whose attr_id == key.
func FindEquipGemRowsByAttrId(key uint32) []*pb.EquipGemTable {
    return EquipGemTableManagerInstance.GetByAttrId(key)
}
