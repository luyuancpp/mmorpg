
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for ActorActionCombatStateTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type ActorActionCombatStateIdComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeActorActionCombatStateIdComp(row *pb.ActorActionCombatStateTable) ActorActionCombatStateIdComp {
    return ActorActionCombatStateIdComp{Value: row.Id}
}

