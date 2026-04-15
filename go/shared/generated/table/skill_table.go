
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



// skillSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type skillSnapshot struct {
    data   []*pb.SkillTable
    kvData map[uint32]*pb.SkillTable
    idxSkill_type map[uint32][]*pb.SkillTable
    idxTargeting_mode map[uint32][]*pb.SkillTable
    idxEffect map[uint32][]*pb.SkillTable
}

type SkillTableManager struct {
    snap *skillSnapshot
}

var SkillTableManagerInstance = NewSkillTableManager()

func NewSkillTableManager() *SkillTableManager {
    return &SkillTableManager{
        snap: &skillSnapshot{
            kvData: make(map[uint32]*pb.SkillTable),
            idxSkill_type: make(map[uint32][]*pb.SkillTable),
            idxTargeting_mode: make(map[uint32][]*pb.SkillTable),
            idxEffect: make(map[uint32][]*pb.SkillTable),
        },
    }
}

func (m *SkillTableManager) Load(configDir string, useBinary bool) error {
    var container pb.SkillTableData

    if useBinary {
        path := filepath.Join(configDir, "skill.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "skill.json")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := protojson.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse json: %w", err)
        }
    }

    snap := &skillSnapshot{
        kvData: make(map[uint32]*pb.SkillTable, len(container.Data)),
        idxSkill_type: make(map[uint32][]*pb.SkillTable),
        idxTargeting_mode: make(map[uint32][]*pb.SkillTable),
        idxEffect: make(map[uint32][]*pb.SkillTable),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
        for _, elem := range row.SkillType {
            snap.idxSkill_type[elem] = append(snap.idxSkill_type[elem], row)
        }
        for _, elem := range row.TargetingMode {
            snap.idxTargeting_mode[elem] = append(snap.idxTargeting_mode[elem], row)
        }
        for _, elem := range row.Effect {
            snap.idxEffect[elem] = append(snap.idxEffect[elem], row)
        }
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *SkillTableManager) FindAll() []*pb.SkillTable {
    return m.snap.data
}

func (m *SkillTableManager) FindById(id uint32) (*pb.SkillTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}


func (m *SkillTableManager) FindBySkill_typeIndex(key uint32) []*pb.SkillTable {
    return m.snap.idxSkill_type[key]
}


func (m *SkillTableManager) FindByTargeting_modeIndex(key uint32) []*pb.SkillTable {
    return m.snap.idxTargeting_mode[key]
}


func (m *SkillTableManager) FindByEffectIndex(key uint32) []*pb.SkillTable {
    return m.snap.idxEffect[key]
}



// ---- Exists ----

func (m *SkillTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *SkillTableManager) Count() int {
    return len(m.snap.data)
}


func (m *SkillTableManager) CountBySkill_typeIndex(key uint32) int {
    return len(m.snap.idxSkill_type[key])
}


func (m *SkillTableManager) CountByTargeting_modeIndex(key uint32) int {
    return len(m.snap.idxTargeting_mode[key])
}


func (m *SkillTableManager) CountByEffectIndex(key uint32) int {
    return len(m.snap.idxEffect[key])
}



// ---- FindByIds (IN) ----

func (m *SkillTableManager) FindByIds(ids []uint32) []*pb.SkillTable {
    result := make([]*pb.SkillTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *SkillTableManager) RandOne() (*pb.SkillTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *SkillTableManager) Where(pred func(*pb.SkillTable) bool) []*pb.SkillTable {
    var result []*pb.SkillTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *SkillTableManager) First(pred func(*pb.SkillTable) bool) (*pb.SkillTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

