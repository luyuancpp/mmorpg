
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



type DungeonTableManager struct {
    data   []*pb.DungeonTable
    kvData map[uint32]*pb.DungeonTable
}

var DungeonTableManagerInstance = NewDungeonTableManager()

func NewDungeonTableManager() *DungeonTableManager {
    return &DungeonTableManager{
        kvData: make(map[uint32]*pb.DungeonTable),
    }
}

func (m *DungeonTableManager) Load(configDir string, useBinary bool) error {
    var container pb.DungeonTableData

    if useBinary {
        path := filepath.Join(configDir, "dungeon.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "dungeon.json")
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

func (m *DungeonTableManager) GetAll() []*pb.DungeonTable {
    return m.data
}

func (m *DungeonTableManager) GetById(id uint32) (*pb.DungeonTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Has / Exists ----

func (m *DungeonTableManager) HasId(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Len / Count ----

func (m *DungeonTableManager) Len() int {
    return len(m.data)
}



// ---- Batch Lookup (IN) ----

func (m *DungeonTableManager) GetByIds(ids []uint32) []*pb.DungeonTable {
    result := make([]*pb.DungeonTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- Random ----

func (m *DungeonTableManager) GetRandom() (*pb.DungeonTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Filter / FindFirst ----

func (m *DungeonTableManager) Filter(pred func(*pb.DungeonTable) bool) []*pb.DungeonTable {
    var result []*pb.DungeonTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *DungeonTableManager) FindFirst(pred func(*pb.DungeonTable) bool) (*pb.DungeonTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}
// FK: scene_id → BaseScene.id


// ---- Composite Key ----

