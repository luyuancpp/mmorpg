
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



// actoractioncombatstateSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type actoractioncombatstateSnapshot struct {
    data   []*pb.ActorActionCombatStateTable
    kvData map[uint32]*pb.ActorActionCombatStateTable
}

type ActorActionCombatStateTableManager struct {
    snap *actoractioncombatstateSnapshot
}

var ActorActionCombatStateTableManagerInstance = NewActorActionCombatStateTableManager()

func NewActorActionCombatStateTableManager() *ActorActionCombatStateTableManager {
    return &ActorActionCombatStateTableManager{
        snap: &actoractioncombatstateSnapshot{
            kvData: make(map[uint32]*pb.ActorActionCombatStateTable),
        },
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

    snap := &actoractioncombatstateSnapshot{
        kvData: make(map[uint32]*pb.ActorActionCombatStateTable, len(container.Data)),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *ActorActionCombatStateTableManager) FindAll() []*pb.ActorActionCombatStateTable {
    return m.snap.data
}

func (m *ActorActionCombatStateTableManager) FindById(id uint32) (*pb.ActorActionCombatStateTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *ActorActionCombatStateTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *ActorActionCombatStateTableManager) Count() int {
    return len(m.snap.data)
}



// ---- FindByIds (IN) ----

func (m *ActorActionCombatStateTableManager) FindByIds(ids []uint32) []*pb.ActorActionCombatStateTable {
    result := make([]*pb.ActorActionCombatStateTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *ActorActionCombatStateTableManager) RandOne() (*pb.ActorActionCombatStateTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *ActorActionCombatStateTableManager) Where(pred func(*pb.ActorActionCombatStateTable) bool) []*pb.ActorActionCombatStateTable {
    var result []*pb.ActorActionCombatStateTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *ActorActionCombatStateTableManager) First(pred func(*pb.ActorActionCombatStateTable) bool) (*pb.ActorActionCombatStateTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

