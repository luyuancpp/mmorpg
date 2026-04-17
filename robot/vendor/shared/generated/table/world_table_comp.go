
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for WorldTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type WorldIdComp struct {
    Value uint32
}

type WorldScene_idComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeWorldIdComp(row *pb.WorldTable) WorldIdComp {
    return WorldIdComp{Value: row.Id}
}

func MakeWorldScene_idComp(row *pb.WorldTable) WorldScene_idComp {
    return WorldScene_idComp{Value: row.SceneId}
}

