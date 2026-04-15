
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



type ActorActionCombatStateTableManager struct {
    data   []*pb.ActorActionCombatStateTable
    kvData map[uint32]*pb.ActorActionCombatStateTable
}

var ActorActionCombatStateTableManagerInstance = NewActorActionCombatStateTableManager()

func NewActorActionCombatStateTableManager() *ActorActionCombatStateTableManager {
    return &ActorActionCombatStateTableManager{
        kvData: make(map[uint32]*pb.ActorActionCombatStateTable),
    }
}

func (m *ActorActionCombatStateTableManager) Load(configDir string, useBinary bool) error {
    var container pb.ActorActionCombatStateTableData

    if useBinary {
        path := filepath.Join(configDir, "actoractioncombatstate.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "actoractioncombatstate.json")
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

func (m *ActorActionCombatStateTableManager) GetAll() []*pb.ActorActionCombatStateTable {
    return m.data
}

func (m *ActorActionCombatStateTableManager) GetById(id uint32) (*pb.ActorActionCombatStateTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Has / Exists ----

func (m *ActorActionCombatStateTableManager) HasId(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Len / Count ----

func (m *ActorActionCombatStateTableManager) Len() int {
    return len(m.data)
}



// ---- Batch Lookup (IN) ----

func (m *ActorActionCombatStateTableManager) GetByIds(ids []uint32) []*pb.ActorActionCombatStateTable {
    result := make([]*pb.ActorActionCombatStateTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- Random ----

func (m *ActorActionCombatStateTableManager) GetRandom() (*pb.ActorActionCombatStateTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Filter / FindFirst ----

func (m *ActorActionCombatStateTableManager) Filter(pred func(*pb.ActorActionCombatStateTable) bool) []*pb.ActorActionCombatStateTable {
    var result []*pb.ActorActionCombatStateTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *ActorActionCombatStateTableManager) FindFirst(pred func(*pb.ActorActionCombatStateTable) bool) (*pb.ActorActionCombatStateTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

