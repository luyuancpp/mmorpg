package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "login/generated/pb/table"
)

var SkillTableManagerInstance = NewSkillTableManager()


type SkillTableManager struct {
    data []*pb.SkillTable
    kvData map[uint32]*pb.SkillTable
}

func NewSkillTableManager() *SkillTableManager {
    return &SkillTableManager{
        kvData: make(map[uint32]*pb.SkillTable),
    }
}

func (m *SkillTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "skill.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.SkillTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *SkillTableManager) GetById(id uint32) (*pb.SkillTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}