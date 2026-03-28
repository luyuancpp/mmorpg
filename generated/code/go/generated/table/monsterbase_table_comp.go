
package table

import (
    pb "game/generated/pb/table"
)

// ============================================================
// Per-column component structs for MonsterBaseTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type MonsterBaseIdComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeMonsterBaseIdComp(row *pb.MonsterBaseTable) MonsterBaseIdComp {
    return MonsterBaseIdComp{Value: row.Id}
}

