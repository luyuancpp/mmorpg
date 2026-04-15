
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for MonsterTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type MonsterIdComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeMonsterIdComp(row *pb.MonsterTable) MonsterIdComp {
    return MonsterIdComp{Value: row.Id}
}

