
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



type MirrorTableManager struct {
    data   []*pb.MirrorTable
    kvData map[uint32]*pb.MirrorTable
}

var MirrorTableManagerInstance = NewMirrorTableManager()

func NewMirrorTableManager() *MirrorTableManager {
    return &MirrorTableManager{
        kvData: make(map[uint32]*pb.MirrorTable),
    }
}

func (m *MirrorTableManager) Load(configDir string, useBinary bool) error {
    var container pb.MirrorTableData

    if useBinary {
        path := filepath.Join(configDir, "mirror.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "mirror.json")
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

func (m *MirrorTableManager) GetAll() []*pb.MirrorTable {
    return m.data
}

func (m *MirrorTableManager) GetById(id uint32) (*pb.MirrorTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Has / Exists ----

func (m *MirrorTableManager) HasId(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Len / Count ----

func (m *MirrorTableManager) Len() int {
    return len(m.data)
}



// ---- Batch Lookup (IN) ----

func (m *MirrorTableManager) GetByIds(ids []uint32) []*pb.MirrorTable {
    result := make([]*pb.MirrorTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- Random ----

func (m *MirrorTableManager) GetRandom() (*pb.MirrorTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Filter / FindFirst ----

func (m *MirrorTableManager) Filter(pred func(*pb.MirrorTable) bool) []*pb.MirrorTable {
    var result []*pb.MirrorTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *MirrorTableManager) FindFirst(pred func(*pb.MirrorTable) bool) (*pb.MirrorTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}
// FK: scene_id → BaseScene.id

// FK: main_scene_id → World.id


// ---- Composite Key ----

