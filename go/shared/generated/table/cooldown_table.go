
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



type CooldownTableManager struct {
    data   []*pb.CooldownTable
    kvData map[uint32]*pb.CooldownTable
}

var CooldownTableManagerInstance = NewCooldownTableManager()

func NewCooldownTableManager() *CooldownTableManager {
    return &CooldownTableManager{
        kvData: make(map[uint32]*pb.CooldownTable),
    }
}

func (m *CooldownTableManager) Load(configDir string, useBinary bool) error {
    var container pb.CooldownTableData

    if useBinary {
        path := filepath.Join(configDir, "cooldown.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "cooldown.json")
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

func (m *CooldownTableManager) FindAll() []*pb.CooldownTable {
    return m.data
}

func (m *CooldownTableManager) FindById(id uint32) (*pb.CooldownTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *CooldownTableManager) Exists(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Count ----

func (m *CooldownTableManager) Count() int {
    return len(m.data)
}



// ---- FindByIds (IN) ----

func (m *CooldownTableManager) FindByIds(ids []uint32) []*pb.CooldownTable {
    result := make([]*pb.CooldownTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *CooldownTableManager) RandOne() (*pb.CooldownTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Where / First ----

func (m *CooldownTableManager) Where(pred func(*pb.CooldownTable) bool) []*pb.CooldownTable {
    var result []*pb.CooldownTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *CooldownTableManager) First(pred func(*pb.CooldownTable) bool) (*pb.CooldownTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

