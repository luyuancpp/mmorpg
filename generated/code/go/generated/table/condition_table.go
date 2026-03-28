
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type ConditionTableManager struct {
    data   []*pb.ConditionTable
    kvData map[uint32]*pb.ConditionTable
    idxCondition1 map[uint32][]*pb.ConditionTable
    idxCondition2 map[uint32][]*pb.ConditionTable
    idxCondition3 map[uint32][]*pb.ConditionTable
    idxCondition4 map[uint32][]*pb.ConditionTable
}

var ConditionTableManagerInstance = NewConditionTableManager()

func NewConditionTableManager() *ConditionTableManager {
    return &ConditionTableManager{
        kvData: make(map[uint32]*pb.ConditionTable),
        idxCondition1: make(map[uint32][]*pb.ConditionTable),
        idxCondition2: make(map[uint32][]*pb.ConditionTable),
        idxCondition3: make(map[uint32][]*pb.ConditionTable),
        idxCondition4: make(map[uint32][]*pb.ConditionTable),
    }
}

func (m *ConditionTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "condition.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %w", err)
    }

    var container pb.ConditionTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
        for _, elem := range row.Condition1 {
            m.idxCondition1[elem] = append(m.idxCondition1[elem], row)
        }
        for _, elem := range row.Condition2 {
            m.idxCondition2[elem] = append(m.idxCondition2[elem], row)
        }
        for _, elem := range row.Condition3 {
            m.idxCondition3[elem] = append(m.idxCondition3[elem], row)
        }
        for _, elem := range row.Condition4 {
            m.idxCondition4[elem] = append(m.idxCondition4[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *ConditionTableManager) GetById(id uint32) (*pb.ConditionTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *ConditionTableManager) GetByCondition1Index(key uint32) []*pb.ConditionTable {
    return m.idxCondition1[key]
}


func (m *ConditionTableManager) GetByCondition2Index(key uint32) []*pb.ConditionTable {
    return m.idxCondition2[key]
}


func (m *ConditionTableManager) GetByCondition3Index(key uint32) []*pb.ConditionTable {
    return m.idxCondition3[key]
}


func (m *ConditionTableManager) GetByCondition4Index(key uint32) []*pb.ConditionTable {
    return m.idxCondition4[key]
}

