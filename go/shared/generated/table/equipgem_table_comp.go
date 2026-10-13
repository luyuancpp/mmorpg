
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for EquipGemTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type EquipGemIdComp struct {
    Value uint32
}

type EquipGemGem_typeComp struct {
    Value uint32
}

type EquipGemAttr_idComp struct {
    Value uint32
}

type EquipGemValueComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeEquipGemIdComp(row *pb.EquipGemTable) EquipGemIdComp {
    return EquipGemIdComp{Value: row.Id}
}

func MakeEquipGemGem_typeComp(row *pb.EquipGemTable) EquipGemGem_typeComp {
    return EquipGemGem_typeComp{Value: row.GemType}
}

func MakeEquipGemAttr_idComp(row *pb.EquipGemTable) EquipGemAttr_idComp {
    return EquipGemAttr_idComp{Value: row.AttrId}
}

func MakeEquipGemValueComp(row *pb.EquipGemTable) EquipGemValueComp {
    return EquipGemValueComp{Value: row.Value}
}

