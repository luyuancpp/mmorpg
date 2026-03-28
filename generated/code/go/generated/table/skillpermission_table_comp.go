
package table

import (
    pb "game/generated/pb/table"
)

// ============================================================
// Per-column component structs for SkillPermissionTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type SkillPermissionIdComp struct {
    Value uint32
}

type SkillPermissionSkill_typeComp struct {
    Values []uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeSkillPermissionIdComp(row *pb.SkillPermissionTable) SkillPermissionIdComp {
    return SkillPermissionIdComp{Value: row.Id}
}

func MakeSkillPermissionSkill_typeComp(row *pb.SkillPermissionTable) SkillPermissionSkill_typeComp {
    return SkillPermissionSkill_typeComp{Values: row.SkillType}
}

