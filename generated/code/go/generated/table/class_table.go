
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
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

func (m *ClassTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "class.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %w", err)
    }

    var container pb.ClassTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %w", err)
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

func (m *ClassTableManager) GetById(id uint32) (*pb.ClassTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *ClassTableManager) GetBySkillIndex(key uint32) []*pb.ClassTable {
    return m.idxSkill[key]
}

