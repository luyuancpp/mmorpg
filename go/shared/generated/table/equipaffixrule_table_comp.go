
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for EquipAffixRuleTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type EquipAffixRuleIdComp struct {
    Value uint32
}

type EquipAffixRuleBlue_minComp struct {
    Value uint32
}

type EquipAffixRuleBlue_maxComp struct {
    Value uint32
}

type EquipAffixRulePink_rateComp struct {
    Value uint32
}

type EquipAffixRuleYellow_rateComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeEquipAffixRuleIdComp(row *pb.EquipAffixRuleTable) EquipAffixRuleIdComp {
    return EquipAffixRuleIdComp{Value: row.Id}
}

func MakeEquipAffixRuleBlue_minComp(row *pb.EquipAffixRuleTable) EquipAffixRuleBlue_minComp {
    return EquipAffixRuleBlue_minComp{Value: row.BlueMin}
}

func MakeEquipAffixRuleBlue_maxComp(row *pb.EquipAffixRuleTable) EquipAffixRuleBlue_maxComp {
    return EquipAffixRuleBlue_maxComp{Value: row.BlueMax}
}

func MakeEquipAffixRulePink_rateComp(row *pb.EquipAffixRuleTable) EquipAffixRulePink_rateComp {
    return EquipAffixRulePink_rateComp{Value: row.PinkRate}
}

func MakeEquipAffixRuleYellow_rateComp(row *pb.EquipAffixRuleTable) EquipAffixRuleYellow_rateComp {
    return EquipAffixRuleYellow_rateComp{Value: row.YellowRate}
}

