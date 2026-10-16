
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for EquipAffixPoolTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type EquipAffixPoolIdComp struct {
    Value uint32
}

type EquipAffixPoolPool_idComp struct {
    Value uint32
}

type EquipAffixPoolAttr_idComp struct {
    Value uint32
}

type EquipAffixPoolWeightComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeEquipAffixPoolIdComp(row *pb.EquipAffixPoolTable) EquipAffixPoolIdComp {
    return EquipAffixPoolIdComp{Value: row.Id}
}

func MakeEquipAffixPoolPool_idComp(row *pb.EquipAffixPoolTable) EquipAffixPoolPool_idComp {
    return EquipAffixPoolPool_idComp{Value: row.PoolId}
}

func MakeEquipAffixPoolAttr_idComp(row *pb.EquipAffixPoolTable) EquipAffixPoolAttr_idComp {
    return EquipAffixPoolAttr_idComp{Value: row.AttrId}
}

func MakeEquipAffixPoolWeightComp(row *pb.EquipAffixPoolTable) EquipAffixPoolWeightComp {
    return EquipAffixPoolWeightComp{Value: row.Weight}
}

