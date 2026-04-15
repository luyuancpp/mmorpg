
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for RewardTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type RewardIdComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeRewardIdComp(row *pb.RewardTable) RewardIdComp {
    return RewardIdComp{Value: row.Id}
}

