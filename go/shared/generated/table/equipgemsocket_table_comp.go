
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for EquipGemSocketTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type EquipGemSocketIdComp struct {
    Value uint32
}

type EquipGemSocketEquip_kindComp struct {
    Value uint32
}

type EquipGemSocketSocketComp struct {
    Value uint32
}

type EquipGemSocketGem_typeComp struct {
    Value uint32
}

type EquipGemSocketNameComp struct {
    Value string
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeEquipGemSocketIdComp(row *pb.EquipGemSocketTable) EquipGemSocketIdComp {
    return EquipGemSocketIdComp{Value: row.Id}
}

func MakeEquipGemSocketEquip_kindComp(row *pb.EquipGemSocketTable) EquipGemSocketEquip_kindComp {
    return EquipGemSocketEquip_kindComp{Value: row.EquipKind}
}

func MakeEquipGemSocketSocketComp(row *pb.EquipGemSocketTable) EquipGemSocketSocketComp {
    return EquipGemSocketSocketComp{Value: row.Socket}
}

func MakeEquipGemSocketGem_typeComp(row *pb.EquipGemSocketTable) EquipGemSocketGem_typeComp {
    return EquipGemSocketGem_typeComp{Value: row.GemType}
}

func MakeEquipGemSocketNameComp(row *pb.EquipGemSocketTable) EquipGemSocketNameComp {
    return EquipGemSocketNameComp{Value: row.Name}
}

