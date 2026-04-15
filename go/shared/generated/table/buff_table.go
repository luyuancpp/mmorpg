
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



type BuffTableManager struct {
    data   []*pb.BuffTable
    kvData map[uint32]*pb.BuffTable
    idxInterval_effect map[float64][]*pb.BuffTable
    idxSub_buff map[uint32][]*pb.BuffTable
    idxTarget_sub_buff map[uint32][]*pb.BuffTable
}

var BuffTableManagerInstance = NewBuffTableManager()

func NewBuffTableManager() *BuffTableManager {
    return &BuffTableManager{
        kvData: make(map[uint32]*pb.BuffTable),
        idxInterval_effect: make(map[float64][]*pb.BuffTable),
        idxSub_buff: make(map[uint32][]*pb.BuffTable),
        idxTarget_sub_buff: make(map[uint32][]*pb.BuffTable),
    }
}

func (m *BuffTableManager) Load(configDir string, useBinary bool) error {
    var container pb.BuffTableData

    if useBinary {
        path := filepath.Join(configDir, "buff.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "buff.json")
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
        for _, elem := range row.IntervalEffect {
            m.idxInterval_effect[elem] = append(m.idxInterval_effect[elem], row)
        }
        for _, elem := range row.SubBuff {
            m.idxSub_buff[elem] = append(m.idxSub_buff[elem], row)
        }
        for _, elem := range row.TargetSubBuff {
            m.idxTarget_sub_buff[elem] = append(m.idxTarget_sub_buff[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *BuffTableManager) GetAll() []*pb.BuffTable {
    return m.data
}

func (m *BuffTableManager) GetById(id uint32) (*pb.BuffTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *BuffTableManager) GetByInterval_effectIndex(key float64) []*pb.BuffTable {
    return m.idxInterval_effect[key]
}


func (m *BuffTableManager) GetBySub_buffIndex(key uint32) []*pb.BuffTable {
    return m.idxSub_buff[key]
}


func (m *BuffTableManager) GetByTarget_sub_buffIndex(key uint32) []*pb.BuffTable {
    return m.idxTarget_sub_buff[key]
}



// ---- Has / Exists ----

func (m *BuffTableManager) HasId(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Len / Count ----

func (m *BuffTableManager) Len() int {
    return len(m.data)
}


func (m *BuffTableManager) CountByInterval_effectIndex(key float64) int {
    return len(m.idxInterval_effect[key])
}


func (m *BuffTableManager) CountBySub_buffIndex(key uint32) int {
    return len(m.idxSub_buff[key])
}


func (m *BuffTableManager) CountByTarget_sub_buffIndex(key uint32) int {
    return len(m.idxTarget_sub_buff[key])
}



// ---- Batch Lookup (IN) ----

func (m *BuffTableManager) GetByIds(ids []uint32) []*pb.BuffTable {
    result := make([]*pb.BuffTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- Random ----

func (m *BuffTableManager) GetRandom() (*pb.BuffTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Filter / FindFirst ----

func (m *BuffTableManager) Filter(pred func(*pb.BuffTable) bool) []*pb.BuffTable {
    var result []*pb.BuffTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *BuffTableManager) FindFirst(pred func(*pb.BuffTable) bool) (*pb.BuffTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

