
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for ConditionTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type ConditionIdComp struct {
    Value uint32
}

type ConditionCondition_categoryComp struct {
    Value uint32
}

type ConditionValid_durationComp struct {
    Value uint64
}

type ConditionQuantity_typeComp struct {
    Value uint32
}

type ConditionTarget_countComp struct {
    Value uint32
}

type ConditionComparison_opComp struct {
    Value uint32
}

type ConditionCondition1Comp struct {
    Values []uint32
}

type ConditionCondition2Comp struct {
    Values []uint32
}

type ConditionCondition3Comp struct {
    Values []uint32
}

type ConditionCondition4Comp struct {
    Values []uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeConditionIdComp(row *pb.ConditionTable) ConditionIdComp {
    return ConditionIdComp{Value: row.Id}
}

func MakeConditionCondition_categoryComp(row *pb.ConditionTable) ConditionCondition_categoryComp {
    return ConditionCondition_categoryComp{Value: row.ConditionCategory}
}

func MakeConditionValid_durationComp(row *pb.ConditionTable) ConditionValid_durationComp {
    return ConditionValid_durationComp{Value: row.ValidDuration}
}

func MakeConditionQuantity_typeComp(row *pb.ConditionTable) ConditionQuantity_typeComp {
    return ConditionQuantity_typeComp{Value: row.QuantityType}
}

func MakeConditionTarget_countComp(row *pb.ConditionTable) ConditionTarget_countComp {
    return ConditionTarget_countComp{Value: row.TargetCount}
}

func MakeConditionComparison_opComp(row *pb.ConditionTable) ConditionComparison_opComp {
    return ConditionComparison_opComp{Value: row.ComparisonOp}
}

func MakeConditionCondition1Comp(row *pb.ConditionTable) ConditionCondition1Comp {
    return ConditionCondition1Comp{Values: row.Condition1}
}

func MakeConditionCondition2Comp(row *pb.ConditionTable) ConditionCondition2Comp {
    return ConditionCondition2Comp{Values: row.Condition2}
}

func MakeConditionCondition3Comp(row *pb.ConditionTable) ConditionCondition3Comp {
    return ConditionCondition3Comp{Values: row.Condition3}
}

func MakeConditionCondition4Comp(row *pb.ConditionTable) ConditionCondition4Comp {
    return ConditionCondition4Comp{Values: row.Condition4}
}

