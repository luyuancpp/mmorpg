
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for CooldownTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type CooldownIdComp struct {
    Value uint32
}

type CooldownDurationComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeCooldownIdComp(row *pb.CooldownTable) CooldownIdComp {
    return CooldownIdComp{Value: row.Id}
}

func MakeCooldownDurationComp(row *pb.CooldownTable) CooldownDurationComp {
    return CooldownDurationComp{Value: row.Duration}
}

