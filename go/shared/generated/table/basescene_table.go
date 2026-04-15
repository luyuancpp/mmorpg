
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



type BaseSceneTableManager struct {
    data   []*pb.BaseSceneTable
    kvData map[uint32]*pb.BaseSceneTable
}

var BaseSceneTableManagerInstance = NewBaseSceneTableManager()

func NewBaseSceneTableManager() *BaseSceneTableManager {
    return &BaseSceneTableManager{
        kvData: make(map[uint32]*pb.BaseSceneTable),
    }
}

func (m *BaseSceneTableManager) Load(configDir string, useBinary bool) error {
    var container pb.BaseSceneTableData

    if useBinary {
        path := filepath.Join(configDir, "basescene.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "basescene.json")
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

func (m *BaseSceneTableManager) GetAll() []*pb.BaseSceneTable {
    return m.data
}

func (m *BaseSceneTableManager) GetById(id uint32) (*pb.BaseSceneTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Has / Exists ----

func (m *BaseSceneTableManager) HasId(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Len / Count ----

func (m *BaseSceneTableManager) Len() int {
    return len(m.data)
}



// ---- Batch Lookup (IN) ----

func (m *BaseSceneTableManager) GetByIds(ids []uint32) []*pb.BaseSceneTable {
    result := make([]*pb.BaseSceneTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- Random ----

func (m *BaseSceneTableManager) GetRandom() (*pb.BaseSceneTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Filter / FindFirst ----

func (m *BaseSceneTableManager) Filter(pred func(*pb.BaseSceneTable) bool) []*pb.BaseSceneTable {
    var result []*pb.BaseSceneTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *BaseSceneTableManager) FindFirst(pred func(*pb.BaseSceneTable) bool) (*pb.BaseSceneTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

