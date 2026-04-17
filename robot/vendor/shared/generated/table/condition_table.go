
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



// conditionSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type conditionSnapshot struct {
    data   []*pb.ConditionTable
    kvData map[uint32]*pb.ConditionTable
    idxCondition1 map[uint32][]*pb.ConditionTable
    idxCondition2 map[uint32][]*pb.ConditionTable
    idxCondition3 map[uint32][]*pb.ConditionTable
    idxCondition4 map[uint32][]*pb.ConditionTable
}

type ConditionTableManager struct {
    snap *conditionSnapshot
}

var ConditionTableManagerInstance = NewConditionTableManager()

func NewConditionTableManager() *ConditionTableManager {
    return &ConditionTableManager{
        snap: &conditionSnapshot{
            kvData: make(map[uint32]*pb.ConditionTable),
            idxCondition1: make(map[uint32][]*pb.ConditionTable),
            idxCondition2: make(map[uint32][]*pb.ConditionTable),
            idxCondition3: make(map[uint32][]*pb.ConditionTable),
            idxCondition4: make(map[uint32][]*pb.ConditionTable),
        },
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

    snap := &conditionSnapshot{
        kvData: make(map[uint32]*pb.ConditionTable, len(container.Data)),
        idxCondition1: make(map[uint32][]*pb.ConditionTable),
        idxCondition2: make(map[uint32][]*pb.ConditionTable),
        idxCondition3: make(map[uint32][]*pb.ConditionTable),
        idxCondition4: make(map[uint32][]*pb.ConditionTable),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
        for _, elem := range row.Condition1 {
            snap.idxCondition1[elem] = append(snap.idxCondition1[elem], row)
        }
        for _, elem := range row.Condition2 {
            snap.idxCondition2[elem] = append(snap.idxCondition2[elem], row)
        }
        for _, elem := range row.Condition3 {
            snap.idxCondition3[elem] = append(snap.idxCondition3[elem], row)
        }
        for _, elem := range row.Condition4 {
            snap.idxCondition4[elem] = append(snap.idxCondition4[elem], row)
        }
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *ConditionTableManager) FindAll() []*pb.ConditionTable {
    return m.snap.data
}

func (m *ConditionTableManager) FindById(id uint32) (*pb.ConditionTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}


func (m *ConditionTableManager) FindByCondition1Index(key uint32) []*pb.ConditionTable {
    return m.snap.idxCondition1[key]
}


func (m *ConditionTableManager) FindByCondition2Index(key uint32) []*pb.ConditionTable {
    return m.snap.idxCondition2[key]
}


func (m *ConditionTableManager) FindByCondition3Index(key uint32) []*pb.ConditionTable {
    return m.snap.idxCondition3[key]
}


func (m *ConditionTableManager) FindByCondition4Index(key uint32) []*pb.ConditionTable {
    return m.snap.idxCondition4[key]
}



// ---- Exists ----

func (m *ConditionTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *ConditionTableManager) Count() int {
    return len(m.snap.data)
}


func (m *ConditionTableManager) CountByCondition1Index(key uint32) int {
    return len(m.snap.idxCondition1[key])
}


func (m *ConditionTableManager) CountByCondition2Index(key uint32) int {
    return len(m.snap.idxCondition2[key])
}


func (m *ConditionTableManager) CountByCondition3Index(key uint32) int {
    return len(m.snap.idxCondition3[key])
}


func (m *ConditionTableManager) CountByCondition4Index(key uint32) int {
    return len(m.snap.idxCondition4[key])
}



// ---- FindByIds (IN) ----

func (m *ConditionTableManager) FindByIds(ids []uint32) []*pb.ConditionTable {
    result := make([]*pb.ConditionTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *ConditionTableManager) RandOne() (*pb.ConditionTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *ConditionTableManager) Where(pred func(*pb.ConditionTable) bool) []*pb.ConditionTable {
    var result []*pb.ConditionTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *ConditionTableManager) First(pred func(*pb.ConditionTable) bool) (*pb.ConditionTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

