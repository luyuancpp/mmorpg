
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



type TestTableManager struct {
    data   []*pb.TestTable
    kvData map[uint32]*pb.TestTable
    idxEffect map[uint32][]*pb.TestTable
}

var TestTableManagerInstance = NewTestTableManager()

func NewTestTableManager() *TestTableManager {
    return &TestTableManager{
        kvData: make(map[uint32]*pb.TestTable),
        idxEffect: make(map[uint32][]*pb.TestTable),
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

    for _, row := range container.Data {
        m.kvData[row.Id] = row
        for _, elem := range row.Effect {
            m.idxEffect[elem] = append(m.idxEffect[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *TestTableManager) GetAll() []*pb.TestTable {
    return m.data
}

func (m *TestTableManager) GetById(id uint32) (*pb.TestTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *TestTableManager) GetByEffectIndex(key uint32) []*pb.TestTable {
    return m.idxEffect[key]
}



// ---- Has / Exists ----

func (m *TestTableManager) HasId(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Len / Count ----

func (m *TestTableManager) Len() int {
    return len(m.data)
}


func (m *TestTableManager) CountByEffectIndex(key uint32) int {
    return len(m.idxEffect[key])
}



// ---- Batch Lookup (IN) ----

func (m *TestTableManager) GetByIds(ids []uint32) []*pb.TestTable {
    result := make([]*pb.TestTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- Random ----

func (m *TestTableManager) GetRandom() (*pb.TestTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Filter / FindFirst ----

func (m *TestTableManager) Filter(pred func(*pb.TestTable) bool) []*pb.TestTable {
    var result []*pb.TestTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *TestTableManager) FindFirst(pred func(*pb.TestTable) bool) (*pb.TestTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

