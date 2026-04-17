
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



// actoractionstateSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type actoractionstateSnapshot struct {
    data   []*pb.ActorActionStateTable
    kvData map[uint32]*pb.ActorActionStateTable
}

type ActorActionStateTableManager struct {
    snap *actoractionstateSnapshot
}

var ActorActionStateTableManagerInstance = NewActorActionStateTableManager()

func NewActorActionStateTableManager() *ActorActionStateTableManager {
    return &ActorActionStateTableManager{
        snap: &actoractionstateSnapshot{
            kvData: make(map[uint32]*pb.ActorActionStateTable),
        },
    }
}

func (m *ActorActionStateTableManager) Load(configDir string, useBinary bool) error {
    var container pb.ActorActionStateTableData

    if useBinary {
        path := filepath.Join(configDir, "actoractionstate.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "actoractionstate.json")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := protojson.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse json: %w", err)
        }
    }

    snap := &actoractionstateSnapshot{
        kvData: make(map[uint32]*pb.ActorActionStateTable, len(container.Data)),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *ActorActionStateTableManager) FindAll() []*pb.ActorActionStateTable {
    return m.snap.data
}

func (m *ActorActionStateTableManager) FindById(id uint32) (*pb.ActorActionStateTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *ActorActionStateTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *ActorActionStateTableManager) Count() int {
    return len(m.snap.data)
}



// ---- FindByIds (IN) ----

func (m *ActorActionStateTableManager) FindByIds(ids []uint32) []*pb.ActorActionStateTable {
    result := make([]*pb.ActorActionStateTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *ActorActionStateTableManager) RandOne() (*pb.ActorActionStateTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *ActorActionStateTableManager) Where(pred func(*pb.ActorActionStateTable) bool) []*pb.ActorActionStateTable {
    var result []*pb.ActorActionStateTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *ActorActionStateTableManager) First(pred func(*pb.ActorActionStateTable) bool) (*pb.ActorActionStateTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

