
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

func (m *ConditionTableManager) Load(configDir string, useBinary bool) error {
    var container pb.ConditionTableData

    if useBinary {
        path := filepath.Join(configDir, "condition.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "condition.json")
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

func (m *ConditionTableManager) FindAll() []*pb.ConditionTable {
    return m.data
}

func (m *ConditionTableManager) FindById(id uint32) (*pb.ConditionTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *ConditionTableManager) FindByCondition1Index(key uint32) []*pb.ConditionTable {
    return m.idxCondition1[key]
}


func (m *ConditionTableManager) FindByCondition2Index(key uint32) []*pb.ConditionTable {
    return m.idxCondition2[key]
}


func (m *ConditionTableManager) FindByCondition3Index(key uint32) []*pb.ConditionTable {
    return m.idxCondition3[key]
}


func (m *ConditionTableManager) FindByCondition4Index(key uint32) []*pb.ConditionTable {
    return m.idxCondition4[key]
}



// ---- Exists ----

func (m *ConditionTableManager) Exists(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Count ----

func (m *ConditionTableManager) Count() int {
    return len(m.data)
}


func (m *ConditionTableManager) CountByCondition1Index(key uint32) int {
    return len(m.idxCondition1[key])
}


func (m *ConditionTableManager) CountByCondition2Index(key uint32) int {
    return len(m.idxCondition2[key])
}


func (m *ConditionTableManager) CountByCondition3Index(key uint32) int {
    return len(m.idxCondition3[key])
}


func (m *ConditionTableManager) CountByCondition4Index(key uint32) int {
    return len(m.idxCondition4[key])
}



// ---- FindByIds (IN) ----

func (m *ConditionTableManager) FindByIds(ids []uint32) []*pb.ConditionTable {
    result := make([]*pb.ConditionTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *ConditionTableManager) RandOne() (*pb.ConditionTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Where / First ----

func (m *ConditionTableManager) Where(pred func(*pb.ConditionTable) bool) []*pb.ConditionTable {
    var result []*pb.ConditionTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *ConditionTableManager) First(pred func(*pb.ConditionTable) bool) (*pb.ConditionTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

