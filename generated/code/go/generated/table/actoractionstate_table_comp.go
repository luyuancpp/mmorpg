
package table

import (
    pb "game/generated/pb/table"
)

// ============================================================
// Per-column component structs for ActorActionStateTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type ActorActionStateIdComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeActorActionStateIdComp(row *pb.ActorActionStateTable) ActorActionStateIdComp {
    return ActorActionStateIdComp{Value: row.Id}
}

