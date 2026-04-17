
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for BaseSceneTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type BaseSceneIdComp struct {
    Value uint32
}

type BaseSceneNav_bin_fileComp struct {
    Value string
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeBaseSceneIdComp(row *pb.BaseSceneTable) BaseSceneIdComp {
    return BaseSceneIdComp{Value: row.Id}
}

func MakeBaseSceneNav_bin_fileComp(row *pb.BaseSceneTable) BaseSceneNav_bin_fileComp {
    return BaseSceneNav_bin_fileComp{Value: row.NavBinFile}
}

