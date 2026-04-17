
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



// skillpermissionSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type skillpermissionSnapshot struct {
    data   []*pb.SkillPermissionTable
    kvData map[uint32]*pb.SkillPermissionTable
    idxSkill_type map[uint32][]*pb.SkillPermissionTable
}

type SkillPermissionTableManager struct {
    snap *skillpermissionSnapshot
}

var SkillPermissionTableManagerInstance = NewSkillPermissionTableManager()

func NewSkillPermissionTableManager() *SkillPermissionTableManager {
    return &SkillPermissionTableManager{
        snap: &skillpermissionSnapshot{
            kvData: make(map[uint32]*pb.SkillPermissionTable),
            idxSkill_type: make(map[uint32][]*pb.SkillPermissionTable),
        },
    }
}

func (m *SkillPermissionTableManager) Load(configDir string, useBinary bool) error {
    var container pb.SkillPermissionTableData

    if useBinary {
        path := filepath.Join(configDir, "skillpermission.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "skillpermission.json")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := protojson.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse json: %w", err)
        }
    }

    snap := &skillpermissionSnapshot{
        kvData: make(map[uint32]*pb.SkillPermissionTable, len(container.Data)),
        idxSkill_type: make(map[uint32][]*pb.SkillPermissionTable),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
        for _, elem := range row.SkillType {
            snap.idxSkill_type[elem] = append(snap.idxSkill_type[elem], row)
        }
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *SkillPermissionTableManager) FindAll() []*pb.SkillPermissionTable {
    return m.snap.data
}

func (m *SkillPermissionTableManager) FindById(id uint32) (*pb.SkillPermissionTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}


func (m *SkillPermissionTableManager) FindBySkill_typeIndex(key uint32) []*pb.SkillPermissionTable {
    return m.snap.idxSkill_type[key]
}



// ---- Exists ----

func (m *SkillPermissionTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *SkillPermissionTableManager) Count() int {
    return len(m.snap.data)
}


func (m *SkillPermissionTableManager) CountBySkill_typeIndex(key uint32) int {
    return len(m.snap.idxSkill_type[key])
}



// ---- FindByIds (IN) ----

func (m *SkillPermissionTableManager) FindByIds(ids []uint32) []*pb.SkillPermissionTable {
    result := make([]*pb.SkillPermissionTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *SkillPermissionTableManager) RandOne() (*pb.SkillPermissionTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *SkillPermissionTableManager) Where(pred func(*pb.SkillPermissionTable) bool) []*pb.SkillPermissionTable {
    var result []*pb.SkillPermissionTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *SkillPermissionTableManager) First(pred func(*pb.SkillPermissionTable) bool) (*pb.SkillPermissionTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

