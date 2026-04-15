package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for TestMultiKeyTable
// ---------------------------------------------------------------------------

// GetTestMultiKeyTestRefRow resolves TestMultiKey.test_ref -> Test row.
func GetTestMultiKeyTestRefRow(row *pb.TestMultiKeyTable) (*pb.TestTable, bool) {
    return TestTableManagerInstance.FindById(row.TestRef)
}

// GetTestMultiKeyTestRefRowById resolves TestMultiKey.test_ref -> Test row (by TestMultiKey id).
func GetTestMultiKeyTestRefRowById(tableId uint32) (*pb.TestTable, bool) {
    row, ok := TestMultiKeyTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetTestMultiKeyTestRefRow(row)
}

// GetTestMultiKeyTestRefsRows resolves TestMultiKey.test_refs[] -> Test rows.
func GetTestMultiKeyTestRefsRows(row *pb.TestMultiKeyTable) []*pb.TestTable {
    var result []*pb.TestTable
    for _, id := range row.TestRefs {
        if r, ok := TestTableManagerInstance.FindById(id); ok {
            result = append(result, r)
        }
    }
    return result
}

// GetTestMultiKeyTestRefsRowsById resolves TestMultiKey.test_refs[] -> Test rows (by TestMultiKey id).
func GetTestMultiKeyTestRefsRowsById(tableId uint32) []*pb.TestTable {
    row, ok := TestMultiKeyTableManagerInstance.FindById(tableId)
    if !ok {
        return nil
    }
    return GetTestMultiKeyTestRefsRows(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindTestMultiKeyRowsByTestRef returns all TestMultiKey rows whose test_ref == key.
func FindTestMultiKeyRowsByTestRef(key uint32) []*pb.TestMultiKeyTable {
    return TestMultiKeyTableManagerInstance.GetByTestRef(key)
}
