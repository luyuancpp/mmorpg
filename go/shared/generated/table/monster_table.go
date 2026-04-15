
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



type MonsterTableManager struct {
    data   []*pb.MonsterTable
    kvData map[uint32]*pb.MonsterTable
}

var MonsterTableManagerInstance = NewMonsterTableManager()

func NewMonsterTableManager() *MonsterTableManager {
    return &MonsterTableManager{
        kvData: make(map[uint32]*pb.MonsterTable),
    }
}

func (m *MonsterTableManager) Load(configDir string, useBinary bool) error {
    var container pb.MonsterTableData

    if useBinary {
        path := filepath.Join(configDir, "monster.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "monster.json")
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

func (m *MonsterTableManager) FindAll() []*pb.MonsterTable {
    return m.data
}

func (m *MonsterTableManager) FindById(id uint32) (*pb.MonsterTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *MonsterTableManager) Exists(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Count ----

func (m *MonsterTableManager) Count() int {
    return len(m.data)
}



// ---- FindByIds (IN) ----

func (m *MonsterTableManager) FindByIds(ids []uint32) []*pb.MonsterTable {
    result := make([]*pb.MonsterTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *MonsterTableManager) RandOne() (*pb.MonsterTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Where / First ----

func (m *MonsterTableManager) Where(pred func(*pb.MonsterTable) bool) []*pb.MonsterTable {
    var result []*pb.MonsterTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *MonsterTableManager) First(pred func(*pb.MonsterTable) bool) (*pb.MonsterTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

