
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type SkillPermissionTableManager struct {
    data   []*pb.SkillPermissionTable
    kvData map[uint32]*pb.SkillPermissionTable
    idxSkill_type map[uint32][]*pb.SkillPermissionTable
}

var SkillPermissionTableManagerInstance = NewSkillPermissionTableManager()

func NewSkillPermissionTableManager() *SkillPermissionTableManager {
    return &SkillPermissionTableManager{
        kvData: make(map[uint32]*pb.SkillPermissionTable),
        idxSkill_type: make(map[uint32][]*pb.SkillPermissionTable),
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

    for _, row := range container.Data {
        m.kvData[row.Id] = row
        for _, elem := range row.SkillType {
            m.idxSkill_type[elem] = append(m.idxSkill_type[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *SkillPermissionTableManager) GetAll() []*pb.SkillPermissionTable {
    return m.data
}

func (m *SkillPermissionTableManager) GetById(id uint32) (*pb.SkillPermissionTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *SkillPermissionTableManager) GetBySkill_typeIndex(key uint32) []*pb.SkillPermissionTable {
    return m.idxSkill_type[key]
}

