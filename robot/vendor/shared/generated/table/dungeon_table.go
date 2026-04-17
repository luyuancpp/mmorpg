
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



// dungeonSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type dungeonSnapshot struct {
    data   []*pb.DungeonTable
    kvData map[uint32]*pb.DungeonTable
    idxSceneId map[uint32][]*pb.DungeonTable
}

type DungeonTableManager struct {
    snap *dungeonSnapshot
}

var DungeonTableManagerInstance = NewDungeonTableManager()

func NewDungeonTableManager() *DungeonTableManager {
    return &DungeonTableManager{
        snap: &dungeonSnapshot{
            kvData: make(map[uint32]*pb.DungeonTable),
            idxSceneId: make(map[uint32][]*pb.DungeonTable),
        },
    }
}

func (m *DungeonTableManager) Load(configDir string, useBinary bool) error {
    var container pb.DungeonTableData

    if useBinary {
        path := filepath.Join(configDir, "dungeon.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "dungeon.json")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := protojson.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse json: %w", err)
        }
    }

    snap := &dungeonSnapshot{
        kvData: make(map[uint32]*pb.DungeonTable, len(container.Data)),
        idxSceneId: make(map[uint32][]*pb.DungeonTable),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
        snap.idxSceneId[row.SceneId] = append(snap.idxSceneId[row.SceneId], row)
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *DungeonTableManager) FindAll() []*pb.DungeonTable {
    return m.snap.data
}

func (m *DungeonTableManager) FindById(id uint32) (*pb.DungeonTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}


func (m *DungeonTableManager) GetBySceneId(key uint32) []*pb.DungeonTable {
    return m.snap.idxSceneId[key]
}



// ---- Exists ----

func (m *DungeonTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *DungeonTableManager) Count() int {
    return len(m.snap.data)
}


func (m *DungeonTableManager) CountBySceneIdIndex(key uint32) int {
    return len(m.snap.idxSceneId[key])
}



// ---- FindByIds (IN) ----

func (m *DungeonTableManager) FindByIds(ids []uint32) []*pb.DungeonTable {
    result := make([]*pb.DungeonTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *DungeonTableManager) RandOne() (*pb.DungeonTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *DungeonTableManager) Where(pred func(*pb.DungeonTable) bool) []*pb.DungeonTable {
    var result []*pb.DungeonTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *DungeonTableManager) First(pred func(*pb.DungeonTable) bool) (*pb.DungeonTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}
// FK: scene_id → BaseScene.id


// ---- Composite Key ----

