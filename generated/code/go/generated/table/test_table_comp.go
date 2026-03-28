
package table

import (
    pb "game/generated/pb/table"
)

// ============================================================
// Per-column component structs for TestTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type TestIdComp struct {
    Value uint32
}

type TestLevelComp struct {
    Value uint32
}

type TestEffectComp struct {
    Values []uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeTestIdComp(row *pb.TestTable) TestIdComp {
    return TestIdComp{Value: row.Id}
}

func MakeTestLevelComp(row *pb.TestTable) TestLevelComp {
    return TestLevelComp{Value: row.Level}
}

func MakeTestEffectComp(row *pb.TestTable) TestEffectComp {
    return TestEffectComp{Values: row.Effect}
}

