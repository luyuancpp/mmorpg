
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for EquipAttributeTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type EquipAttributeIdComp struct {
    Value uint32
}

type EquipAttributeNameComp struct {
    Value string
}

type EquipAttributeEffectComp struct {
    Value uint32
}

type EquipAttributeEffect_paramComp struct {
    Value uint32
}

type EquipAttributePercentComp struct {
    Value uint32
}

type EquipAttributeSortComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeEquipAttributeIdComp(row *pb.EquipAttributeTable) EquipAttributeIdComp {
    return EquipAttributeIdComp{Value: row.Id}
}

func MakeEquipAttributeNameComp(row *pb.EquipAttributeTable) EquipAttributeNameComp {
    return EquipAttributeNameComp{Value: row.Name}
}

func MakeEquipAttributeEffectComp(row *pb.EquipAttributeTable) EquipAttributeEffectComp {
    return EquipAttributeEffectComp{Value: row.Effect}
}

func MakeEquipAttributeEffect_paramComp(row *pb.EquipAttributeTable) EquipAttributeEffect_paramComp {
    return EquipAttributeEffect_paramComp{Value: row.EffectParam}
}

func MakeEquipAttributePercentComp(row *pb.EquipAttributeTable) EquipAttributePercentComp {
    return EquipAttributePercentComp{Value: row.Percent}
}

func MakeEquipAttributeSortComp(row *pb.EquipAttributeTable) EquipAttributeSortComp {
    return EquipAttributeSortComp{Value: row.Sort}
}

