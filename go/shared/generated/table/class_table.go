
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



// classSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type classSnapshot struct {
    data   []*pb.ClassTable
    kvData map[uint32]*pb.ClassTable
    idxSkill map[uint32][]*pb.ClassTable
}

type ClassTableManager struct {
    snap *classSnapshot
}

var ClassTableManagerInstance = NewClassTableManager()

func NewClassTableManager() *ClassTableManager {
    return &ClassTableManager{
        snap: &classSnapshot{
            kvData: make(map[uint32]*pb.ClassTable),
            idxSkill: make(map[uint32][]*pb.ClassTable),
        },
    }
}

func (m *ClassTableManager) Load(configDir string, useBinary bool) error {
    var container pb.ClassTableData

    if useBinary {
        path := filepath.Join(configDir, "class.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "class.json")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := protojson.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse json: %w", err)
        }
    }

    snap := &classSnapshot{
        kvData: make(map[uint32]*pb.ClassTable, len(container.Data)),
        idxSkill: make(map[uint32][]*pb.ClassTable),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
        for _, elem := range row.Skill {
            snap.idxSkill[elem] = append(snap.idxSkill[elem], row)
        }
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *ClassTableManager) FindAll() []*pb.ClassTable {
    return m.snap.data
}

func (m *ClassTableManager) FindById(id uint32) (*pb.ClassTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}


func (m *ClassTableManager) FindBySkillIndex(key uint32) []*pb.ClassTable {
    return m.snap.idxSkill[key]
}



// ---- Exists ----

func (m *ClassTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *ClassTableManager) Count() int {
    return len(m.snap.data)
}


func (m *ClassTableManager) CountBySkillIndex(key uint32) int {
    return len(m.snap.idxSkill[key])
}



// ---- FindByIds (IN) ----

func (m *ClassTableManager) FindByIds(ids []uint32) []*pb.ClassTable {
    result := make([]*pb.ClassTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *ClassTableManager) RandOne() (*pb.ClassTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *ClassTableManager) Where(pred func(*pb.ClassTable) bool) []*pb.ClassTable {
    var result []*pb.ClassTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *ClassTableManager) First(pred func(*pb.ClassTable) bool) (*pb.ClassTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

