
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



// worldSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type worldSnapshot struct {
    data   []*pb.WorldTable
    kvData map[uint32]*pb.WorldTable
}

type WorldTableManager struct {
    snap *worldSnapshot
}

var WorldTableManagerInstance = NewWorldTableManager()

func NewWorldTableManager() *WorldTableManager {
    return &WorldTableManager{
        snap: &worldSnapshot{
            kvData: make(map[uint32]*pb.WorldTable),
        },
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

    snap := &worldSnapshot{
        kvData: make(map[uint32]*pb.WorldTable, len(container.Data)),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *WorldTableManager) FindAll() []*pb.WorldTable {
    return m.snap.data
}

func (m *WorldTableManager) FindById(id uint32) (*pb.WorldTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *WorldTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *WorldTableManager) Count() int {
    return len(m.snap.data)
}



// ---- FindByIds (IN) ----

func (m *WorldTableManager) FindByIds(ids []uint32) []*pb.WorldTable {
    result := make([]*pb.WorldTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *WorldTableManager) RandOne() (*pb.WorldTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *WorldTableManager) Where(pred func(*pb.WorldTable) bool) []*pb.WorldTable {
    var result []*pb.WorldTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *WorldTableManager) First(pred func(*pb.WorldTable) bool) (*pb.WorldTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}
// FK: scene_id → BaseScene.id


// ---- Composite Key ----

