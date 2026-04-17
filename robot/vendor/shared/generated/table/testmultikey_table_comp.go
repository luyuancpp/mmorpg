
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for TestMultiKeyTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type TestMultiKeyIdComp struct {
    Value uint32
}

type TestMultiKeyLevelComp struct {
    Value uint32
}

type TestMultiKeyString_keyComp struct {
    Value string
}

type TestMultiKeyUint32_keyComp struct {
    Value uint32
}

type TestMultiKeyInt32_keyComp struct {
    Value int32
}

type TestMultiKeyM_string_keyComp struct {
    Value string
}

type TestMultiKeyM_uint32_keyComp struct {
    Value uint32
}

type TestMultiKeyM_int32_keyComp struct {
    Value int32
}

type TestMultiKeyTest_refComp struct {
    Value uint32
}

type TestMultiKeyEffectComp struct {
    Values []uint32
}

type TestMultiKeyTest_refsComp struct {
    Values []uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeTestMultiKeyIdComp(row *pb.TestMultiKeyTable) TestMultiKeyIdComp {
    return TestMultiKeyIdComp{Value: row.Id}
}

func MakeTestMultiKeyLevelComp(row *pb.TestMultiKeyTable) TestMultiKeyLevelComp {
    return TestMultiKeyLevelComp{Value: row.Level}
}

func MakeTestMultiKeyString_keyComp(row *pb.TestMultiKeyTable) TestMultiKeyString_keyComp {
    return TestMultiKeyString_keyComp{Value: row.StringKey}
}

func MakeTestMultiKeyUint32_keyComp(row *pb.TestMultiKeyTable) TestMultiKeyUint32_keyComp {
    return TestMultiKeyUint32_keyComp{Value: row.Uint32Key}
}

func MakeTestMultiKeyInt32_keyComp(row *pb.TestMultiKeyTable) TestMultiKeyInt32_keyComp {
    return TestMultiKeyInt32_keyComp{Value: row.Int32Key}
}

func MakeTestMultiKeyM_string_keyComp(row *pb.TestMultiKeyTable) TestMultiKeyM_string_keyComp {
    return TestMultiKeyM_string_keyComp{Value: row.MStringKey}
}

func MakeTestMultiKeyM_uint32_keyComp(row *pb.TestMultiKeyTable) TestMultiKeyM_uint32_keyComp {
    return TestMultiKeyM_uint32_keyComp{Value: row.MUint32Key}
}

func MakeTestMultiKeyM_int32_keyComp(row *pb.TestMultiKeyTable) TestMultiKeyM_int32_keyComp {
    return TestMultiKeyM_int32_keyComp{Value: row.MInt32Key}
}

func MakeTestMultiKeyTest_refComp(row *pb.TestMultiKeyTable) TestMultiKeyTest_refComp {
    return TestMultiKeyTest_refComp{Value: row.TestRef}
}

func MakeTestMultiKeyEffectComp(row *pb.TestMultiKeyTable) TestMultiKeyEffectComp {
    return TestMultiKeyEffectComp{Values: row.Effect}
}

func MakeTestMultiKeyTest_refsComp(row *pb.TestMultiKeyTable) TestMultiKeyTest_refsComp {
    return TestMultiKeyTest_refsComp{Values: row.TestRefs}
}

