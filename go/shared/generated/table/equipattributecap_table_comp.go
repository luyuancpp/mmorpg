
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for EquipAttributeCapTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type EquipAttributeCapIdComp struct {
    Value uint32
}

type EquipAttributeCapAttr_idComp struct {
    Value uint32
}

type EquipAttributeCapLevelComp struct {
    Value uint32
}

type EquipAttributeCapMin_valueComp struct {
    Value uint32
}

type EquipAttributeCapCapComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeEquipAttributeCapIdComp(row *pb.EquipAttributeCapTable) EquipAttributeCapIdComp {
    return EquipAttributeCapIdComp{Value: row.Id}
}

func MakeEquipAttributeCapAttr_idComp(row *pb.EquipAttributeCapTable) EquipAttributeCapAttr_idComp {
    return EquipAttributeCapAttr_idComp{Value: row.AttrId}
}

func MakeEquipAttributeCapLevelComp(row *pb.EquipAttributeCapTable) EquipAttributeCapLevelComp {
    return EquipAttributeCapLevelComp{Value: row.Level}
}

func MakeEquipAttributeCapMin_valueComp(row *pb.EquipAttributeCapTable) EquipAttributeCapMin_valueComp {
    return EquipAttributeCapMin_valueComp{Value: row.MinValue}
}

func MakeEquipAttributeCapCapComp(row *pb.EquipAttributeCapTable) EquipAttributeCapCapComp {
    return EquipAttributeCapCapComp{Value: row.Cap}
}

