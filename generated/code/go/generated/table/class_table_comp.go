
package table

import (
    pb "game/generated/pb/table"
)

// ============================================================
// Per-column component structs for ClassTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type ClassIdComp struct {
    Value uint32
}

type ClassSkillComp struct {
    Values []uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeClassIdComp(row *pb.ClassTable) ClassIdComp {
    return ClassIdComp{Value: row.Id}
}

func MakeClassSkillComp(row *pb.ClassTable) ClassSkillComp {
    return ClassSkillComp{Values: row.Skill}
}

