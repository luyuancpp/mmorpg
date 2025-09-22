package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type SkillPermissionTableManager struct {
    data []*pb.SkillPermissionTable
    kvData map[int32]*pb.SkillPermissionTable
}

func NewSkillPermissionTableManager() *SkillPermissionTableManager {
    return &SkillPermissionTableManager{
        kvData: make(map[int32]*pb.SkillPermissionTable),
    }
}

func (m *SkillPermissionTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "skillpermission.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.SkillPermissionTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *SkillPermissionTableManager) GetById(id int32) (*pb.SkillPermission, bool) {
    row, ok := m.kvData[id]
    return row, ok
}