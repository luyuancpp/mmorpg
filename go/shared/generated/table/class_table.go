
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



type ClassTableManager struct {
    data   []*pb.ClassTable
    kvData map[uint32]*pb.ClassTable
    idxSkill map[uint32][]*pb.ClassTable
}

var ClassTableManagerInstance = NewClassTableManager()

func NewClassTableManager() *ClassTableManager {
    return &ClassTableManager{
        kvData: make(map[uint32]*pb.ClassTable),
        idxSkill: make(map[uint32][]*pb.ClassTable),
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

    for _, row := range container.Data {
        m.kvData[row.Id] = row
        for _, elem := range row.Skill {
            m.idxSkill[elem] = append(m.idxSkill[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *ClassTableManager) FindAll() []*pb.ClassTable {
    return m.data
}

func (m *ClassTableManager) FindById(id uint32) (*pb.ClassTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *ClassTableManager) FindBySkillIndex(key uint32) []*pb.ClassTable {
    return m.idxSkill[key]
}



// ---- Exists ----

func (m *ClassTableManager) Exists(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Count ----

func (m *ClassTableManager) Count() int {
    return len(m.data)
}


func (m *ClassTableManager) CountBySkillIndex(key uint32) int {
    return len(m.idxSkill[key])
}



// ---- FindByIds (IN) ----

func (m *ClassTableManager) FindByIds(ids []uint32) []*pb.ClassTable {
    result := make([]*pb.ClassTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *ClassTableManager) RandOne() (*pb.ClassTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Where / First ----

func (m *ClassTableManager) Where(pred func(*pb.ClassTable) bool) []*pb.ClassTable {
    var result []*pb.ClassTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *ClassTableManager) First(pred func(*pb.ClassTable) bool) (*pb.ClassTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

