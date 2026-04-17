
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for GlobalVariableTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type GlobalVariableIdComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeGlobalVariableIdComp(row *pb.GlobalVariableTable) GlobalVariableIdComp {
    return GlobalVariableIdComp{Value: row.Id}
}

