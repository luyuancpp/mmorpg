
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



type SkillTableManager struct {
    data   []*pb.SkillTable
    kvData map[uint32]*pb.SkillTable
    idxSkill_type map[uint32][]*pb.SkillTable
    idxTargeting_mode map[uint32][]*pb.SkillTable
    idxEffect map[uint32][]*pb.SkillTable
}

var SkillTableManagerInstance = NewSkillTableManager()

func NewSkillTableManager() *SkillTableManager {
    return &SkillTableManager{
        kvData: make(map[uint32]*pb.SkillTable),
        idxSkill_type: make(map[uint32][]*pb.SkillTable),
        idxTargeting_mode: make(map[uint32][]*pb.SkillTable),
        idxEffect: make(map[uint32][]*pb.SkillTable),
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

    for _, row := range container.Data {
        m.kvData[row.Id] = row
        for _, elem := range row.SkillType {
            m.idxSkill_type[elem] = append(m.idxSkill_type[elem], row)
        }
        for _, elem := range row.TargetingMode {
            m.idxTargeting_mode[elem] = append(m.idxTargeting_mode[elem], row)
        }
        for _, elem := range row.Effect {
            m.idxEffect[elem] = append(m.idxEffect[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *SkillTableManager) GetAll() []*pb.SkillTable {
    return m.data
}

func (m *SkillTableManager) GetById(id uint32) (*pb.SkillTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *SkillTableManager) GetBySkill_typeIndex(key uint32) []*pb.SkillTable {
    return m.idxSkill_type[key]
}


func (m *SkillTableManager) GetByTargeting_modeIndex(key uint32) []*pb.SkillTable {
    return m.idxTargeting_mode[key]
}


func (m *SkillTableManager) GetByEffectIndex(key uint32) []*pb.SkillTable {
    return m.idxEffect[key]
}



// ---- Has / Exists ----

func (m *SkillTableManager) HasId(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Len / Count ----

func (m *SkillTableManager) Len() int {
    return len(m.data)
}


func (m *SkillTableManager) CountBySkill_typeIndex(key uint32) int {
    return len(m.idxSkill_type[key])
}


func (m *SkillTableManager) CountByTargeting_modeIndex(key uint32) int {
    return len(m.idxTargeting_mode[key])
}


func (m *SkillTableManager) CountByEffectIndex(key uint32) int {
    return len(m.idxEffect[key])
}



// ---- Batch Lookup (IN) ----

func (m *SkillTableManager) GetByIds(ids []uint32) []*pb.SkillTable {
    result := make([]*pb.SkillTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- Random ----

func (m *SkillTableManager) GetRandom() (*pb.SkillTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Filter / FindFirst ----

func (m *SkillTableManager) Filter(pred func(*pb.SkillTable) bool) []*pb.SkillTable {
    var result []*pb.SkillTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *SkillTableManager) FindFirst(pred func(*pb.SkillTable) bool) (*pb.SkillTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

