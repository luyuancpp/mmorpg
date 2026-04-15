
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



// mirrorSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type mirrorSnapshot struct {
    data   []*pb.MirrorTable
    kvData map[uint32]*pb.MirrorTable
}

type MirrorTableManager struct {
    snap *mirrorSnapshot
}

var MirrorTableManagerInstance = NewMirrorTableManager()

func NewMirrorTableManager() *MirrorTableManager {
    return &MirrorTableManager{
        snap: &mirrorSnapshot{
            kvData: make(map[uint32]*pb.MirrorTable),
        },
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

    snap := &mirrorSnapshot{
        kvData: make(map[uint32]*pb.MirrorTable, len(container.Data)),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *MirrorTableManager) FindAll() []*pb.MirrorTable {
    return m.snap.data
}

func (m *MirrorTableManager) FindById(id uint32) (*pb.MirrorTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *MirrorTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *MirrorTableManager) Count() int {
    return len(m.snap.data)
}



// ---- FindByIds (IN) ----

func (m *MirrorTableManager) FindByIds(ids []uint32) []*pb.MirrorTable {
    result := make([]*pb.MirrorTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *MirrorTableManager) RandOne() (*pb.MirrorTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *MirrorTableManager) Where(pred func(*pb.MirrorTable) bool) []*pb.MirrorTable {
    var result []*pb.MirrorTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *MirrorTableManager) First(pred func(*pb.MirrorTable) bool) (*pb.MirrorTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}
// FK: scene_id → BaseScene.id

// FK: main_scene_id → World.id


// ---- Composite Key ----

