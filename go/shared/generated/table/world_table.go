
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



type WorldTableManager struct {
    data   []*pb.WorldTable
    kvData map[uint32]*pb.WorldTable
}

var WorldTableManagerInstance = NewWorldTableManager()

func NewWorldTableManager() *WorldTableManager {
    return &WorldTableManager{
        kvData: make(map[uint32]*pb.WorldTable),
    }
}

func (m *WorldTableManager) Load(configDir string, useBinary bool) error {
    var container pb.WorldTableData

    if useBinary {
        path := filepath.Join(configDir, "world.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "world.json")
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
    }

    m.data = container.Data
    return nil
}

func (m *WorldTableManager) GetAll() []*pb.WorldTable {
    return m.data
}

func (m *WorldTableManager) GetById(id uint32) (*pb.WorldTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Has / Exists ----

func (m *WorldTableManager) HasId(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Len / Count ----

func (m *WorldTableManager) Len() int {
    return len(m.data)
}



// ---- Batch Lookup (IN) ----

func (m *WorldTableManager) GetByIds(ids []uint32) []*pb.WorldTable {
    result := make([]*pb.WorldTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- Random ----

func (m *WorldTableManager) GetRandom() (*pb.WorldTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Filter / FindFirst ----

func (m *WorldTableManager) Filter(pred func(*pb.WorldTable) bool) []*pb.WorldTable {
    var result []*pb.WorldTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *WorldTableManager) FindFirst(pred func(*pb.WorldTable) bool) (*pb.WorldTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}
// FK: scene_id → BaseScene.id


// ---- Composite Key ----

