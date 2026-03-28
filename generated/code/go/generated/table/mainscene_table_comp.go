
package table

import (
    pb "game/generated/pb/table"
)

// ============================================================
// Per-column component structs for MainSceneTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type MainSceneIdComp struct {
    Value uint32
}

type MainSceneNav_bin_fileComp struct {
    Value string
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeMainSceneIdComp(row *pb.MainSceneTable) MainSceneIdComp {
    return MainSceneIdComp{Value: row.Id}
}

func MakeMainSceneNav_bin_fileComp(row *pb.MainSceneTable) MainSceneNav_bin_fileComp {
    return MainSceneNav_bin_fileComp{Value: row.NavBinFile}
}

