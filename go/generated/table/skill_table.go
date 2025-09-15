package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "your/proto/package/path"
)

type SkillTableManager struct {
    data []*pb.Skill
    kvData map[int32]*pb.Skill
}

func NewSkillTableManager() *SkillTableManager {
    return &SkillTableManager{
        kvData: make(map[int32]*pb.Skill),
    }
}

func (m *SkillTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "skill.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.SkillList
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *SkillTableManager) GetById(id int32) (*pb.Skill, bool) {
    row, ok := m.kvData[id]
    return row, ok
}