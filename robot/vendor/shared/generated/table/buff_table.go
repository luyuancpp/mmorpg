
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



// buffSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type buffSnapshot struct {
    data   []*pb.BuffTable
    kvData map[uint32]*pb.BuffTable
    idxInterval_effect map[float64][]*pb.BuffTable
    idxSub_buff map[uint32][]*pb.BuffTable
    idxTarget_sub_buff map[uint32][]*pb.BuffTable
}

type BuffTableManager struct {
    snap *buffSnapshot
}

var BuffTableManagerInstance = NewBuffTableManager()

func NewBuffTableManager() *BuffTableManager {
    return &BuffTableManager{
        snap: &buffSnapshot{
            kvData: make(map[uint32]*pb.BuffTable),
            idxInterval_effect: make(map[float64][]*pb.BuffTable),
            idxSub_buff: make(map[uint32][]*pb.BuffTable),
            idxTarget_sub_buff: make(map[uint32][]*pb.BuffTable),
        },
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

    snap := &buffSnapshot{
        kvData: make(map[uint32]*pb.BuffTable, len(container.Data)),
        idxInterval_effect: make(map[float64][]*pb.BuffTable),
        idxSub_buff: make(map[uint32][]*pb.BuffTable),
        idxTarget_sub_buff: make(map[uint32][]*pb.BuffTable),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
        for _, elem := range row.IntervalEffect {
            snap.idxInterval_effect[elem] = append(snap.idxInterval_effect[elem], row)
        }
        for _, elem := range row.SubBuff {
            snap.idxSub_buff[elem] = append(snap.idxSub_buff[elem], row)
        }
        for _, elem := range row.TargetSubBuff {
            snap.idxTarget_sub_buff[elem] = append(snap.idxTarget_sub_buff[elem], row)
        }
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *BuffTableManager) FindAll() []*pb.BuffTable {
    return m.snap.data
}

func (m *BuffTableManager) FindById(id uint32) (*pb.BuffTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}


func (m *BuffTableManager) FindByInterval_effectIndex(key float64) []*pb.BuffTable {
    return m.snap.idxInterval_effect[key]
}


func (m *BuffTableManager) FindBySub_buffIndex(key uint32) []*pb.BuffTable {
    return m.snap.idxSub_buff[key]
}


func (m *BuffTableManager) FindByTarget_sub_buffIndex(key uint32) []*pb.BuffTable {
    return m.snap.idxTarget_sub_buff[key]
}



// ---- Exists ----

func (m *BuffTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *BuffTableManager) Count() int {
    return len(m.snap.data)
}


func (m *BuffTableManager) CountByInterval_effectIndex(key float64) int {
    return len(m.snap.idxInterval_effect[key])
}


func (m *BuffTableManager) CountBySub_buffIndex(key uint32) int {
    return len(m.snap.idxSub_buff[key])
}


func (m *BuffTableManager) CountByTarget_sub_buffIndex(key uint32) int {
    return len(m.snap.idxTarget_sub_buff[key])
}



// ---- FindByIds (IN) ----

func (m *BuffTableManager) FindByIds(ids []uint32) []*pb.BuffTable {
    result := make([]*pb.BuffTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *BuffTableManager) RandOne() (*pb.BuffTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *BuffTableManager) Where(pred func(*pb.BuffTable) bool) []*pb.BuffTable {
    var result []*pb.BuffTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *BuffTableManager) First(pred func(*pb.BuffTable) bool) (*pb.BuffTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

