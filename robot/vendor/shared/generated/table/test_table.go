
package table

import (
    "fmt"
    "math/rand/v2"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)



// testSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type testSnapshot struct {
    data   []*pb.TestTable
    kvData map[uint32]*pb.TestTable
    idxEffect map[uint32][]*pb.TestTable
}

type TestTableManager struct {
    snap *testSnapshot
}

var TestTableManagerInstance = NewTestTableManager()

func NewTestTableManager() *TestTableManager {
    return &TestTableManager{
        snap: &testSnapshot{
            kvData: make(map[uint32]*pb.TestTable),
            idxEffect: make(map[uint32][]*pb.TestTable),
        },
    }
}

func (m *TestTableManager) Load(configDir string, useBinary bool) error {
    var container pb.TestTableData

    if useBinary {
        path := filepath.Join(configDir, "test.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "test.json")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := protojson.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse json: %w", err)
        }
    }

    snap := &testSnapshot{
        kvData: make(map[uint32]*pb.TestTable, len(container.Data)),
        idxEffect: make(map[uint32][]*pb.TestTable),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
        for _, elem := range row.Effect {
            snap.idxEffect[elem] = append(snap.idxEffect[elem], row)
        }
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *TestTableManager) FindAll() []*pb.TestTable {
    return m.snap.data
}

func (m *TestTableManager) FindById(id uint32) (*pb.TestTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}


func (m *TestTableManager) FindByEffectIndex(key uint32) []*pb.TestTable {
    return m.snap.idxEffect[key]
}



// ---- Exists ----

func (m *TestTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *TestTableManager) Count() int {
    return len(m.snap.data)
}


func (m *TestTableManager) CountByEffectIndex(key uint32) int {
    return len(m.snap.idxEffect[key])
}



// ---- FindByIds (IN) ----

func (m *TestTableManager) FindByIds(ids []uint32) []*pb.TestTable {
    result := make([]*pb.TestTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *TestTableManager) RandOne() (*pb.TestTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *TestTableManager) Where(pred func(*pb.TestTable) bool) []*pb.TestTable {
    var result []*pb.TestTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *TestTableManager) First(pred func(*pb.TestTable) bool) (*pb.TestTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

