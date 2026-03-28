
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
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

func (m *TestTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "test.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %w", err)
    }

    var container pb.TestTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %w", err)
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

func (m *TestTableManager) GetById(id uint32) (*pb.TestTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *TestTableManager) GetByEffectIndex(key uint32) []*pb.TestTable {
    return m.idxEffect[key]
}

