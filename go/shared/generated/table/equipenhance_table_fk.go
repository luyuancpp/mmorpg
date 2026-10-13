package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for EquipEnhanceTable
// ---------------------------------------------------------------------------

// GetEquipEnhanceMaterialIdRow resolves EquipEnhance.material_id -> Item row.
func GetEquipEnhanceMaterialIdRow(row *pb.EquipEnhanceTable) (*pb.ItemTable, bool) {
    return ItemTableManagerInstance.FindById(row.MaterialId)
}

// GetEquipEnhanceMaterialIdRowById resolves EquipEnhance.material_id -> Item row (by EquipEnhance id).
func GetEquipEnhanceMaterialIdRowById(tableId uint32) (*pb.ItemTable, bool) {
    row, ok := EquipEnhanceTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetEquipEnhanceMaterialIdRow(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindEquipEnhanceRowsByMaterialId returns all EquipEnhance rows whose material_id == key.
func FindEquipEnhanceRowsByMaterialId(key uint32) []*pb.EquipEnhanceTable {
    return EquipEnhanceTableManagerInstance.GetByMaterialId(key)
}
