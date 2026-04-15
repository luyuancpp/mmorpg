
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



type ItemTableManager struct {
    data   []*pb.ItemTable
    kvData map[uint32]*pb.ItemTable
}

var ItemTableManagerInstance = NewItemTableManager()

func NewItemTableManager() *ItemTableManager {
    return &ItemTableManager{
        kvData: make(map[uint32]*pb.ItemTable),
    }
}

func (m *ItemTableManager) Load(configDir string, useBinary bool) error {
    var container pb.ItemTableData

    if useBinary {
        path := filepath.Join(configDir, "item.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "item.json")
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

func (m *ItemTableManager) FindAll() []*pb.ItemTable {
    return m.data
}

func (m *ItemTableManager) FindById(id uint32) (*pb.ItemTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *ItemTableManager) Exists(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Count ----

func (m *ItemTableManager) Count() int {
    return len(m.data)
}



// ---- FindByIds (IN) ----

func (m *ItemTableManager) FindByIds(ids []uint32) []*pb.ItemTable {
    result := make([]*pb.ItemTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *ItemTableManager) RandOne() (*pb.ItemTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Where / First ----

func (m *ItemTableManager) Where(pred func(*pb.ItemTable) bool) []*pb.ItemTable {
    var result []*pb.ItemTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *ItemTableManager) First(pred func(*pb.ItemTable) bool) (*pb.ItemTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

